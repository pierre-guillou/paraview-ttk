/*=========================================================================

  Program:   ParaView
  Module:    vtkPVPluginTracker.cxx

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkPVPluginTracker.h"

#include "vtkClientServerInterpreterInitializer.h"
#include "vtkCommand.h"
#include "vtkObjectFactory.h"
#include "vtkPResourceFileLocator.h"
#include "vtkPSystemTools.h"
#include "vtkPVConfig.h"
#include "vtkPVOptions.h"
#include "vtkPVPlugin.h"
#include "vtkPVPluginLoader.h"
#include "vtkPVPythonModule.h"
#include "vtkPVPythonPluginInterface.h"
#include "vtkPVServerManagerPluginInterface.h"
#include "vtkPVXMLElement.h"
#include "vtkPVXMLParser.h"
#include "vtkProcessModule.h"
#include "vtkVersion.h"

#include <assert.h>
#include <sstream>
#include <string>
#include <vector>
#include <vtksys/String.hxx>
#include <vtksys/SystemTools.hxx>

#if defined(_WIN32) && !defined(__CYGWIN__)
/* String comparison routine. */
#define VTKSTRNCASECMP _strnicmp
#else
#include "strings.h"
#define VTKSTRNCASECMP strncasecmp
#endif

#define vtkPVPluginTrackerDebugMacro(x)                                                            \
  {                                                                                                \
    if (debug_plugin)                                                                              \
    {                                                                                              \
      std::ostringstream vtkerror;                                                                 \
      vtkerror << x << endl;                                                                       \
      vtkOutputWindowDisplayText(vtkerror.str().c_str());                                          \
    }                                                                                              \
  }

namespace
{
class vtkItem
{
public:
  std::string FileName;
  std::string PluginName;
  vtkPVPlugin* Plugin;
  bool AutoLoad;
  vtkItem()
  {
    this->Plugin = NULL;
    this->AutoLoad = false;
  }
};

/**
 * Convert a plugin name to its library name i.e. add platform specific
 * library prefix and suffix.
 */
std::string vtkGetPluginFileNameFromName(const std::string& pluginname)
{
#if defined(_WIN32) && !defined(__CYGWIN__)
  return pluginname + ".dll";
#elif defined(__APPLE__)
  return "lib" + pluginname + ".dylib";
#else
  return "lib" + pluginname + ".so";
#endif
}

/**
 * Locate a plugin library or a config file anchored at standard locations
 * for locating plugins.
 */
std::string vtkLocatePluginOrConfigFile(
  const char* plugin, bool isPlugin, vtkPluginSearchFunction searchFunction)
{
  (void)searchFunction;
  auto pm = vtkProcessModule::GetProcessModule();
  // Make sure we can get the options before going further
  if (pm == NULL)
  {
    return std::string();
  }

  bool debug_plugin = vtksys::SystemTools::GetEnv("PV_PLUGIN_DEBUG") != NULL;
#ifndef BUILD_SHARED_LIBS
  if (isPlugin)
  {
    vtkPVPluginTrackerDebugMacro("Looking for static plugin \'" << plugin << "\'");
    if (searchFunction && searchFunction(plugin))
    {
      vtkPVPluginTrackerDebugMacro("Found static plugin \'" << plugin << "\'");
      return plugin;
    }
  }
#endif

  const std::string exe_dir = pm->GetSelfDir();
  const std::string vtklib = vtkGetLibraryPathForSymbol(GetVTKVersion);

  std::vector<std::string> prefixes = {
#if defined(BUILD_SHARED_LIBS)
    std::string("paraview-" PARAVIEW_VERSION "/plugins/") + plugin,
    std::string("paraview-" PARAVIEW_VERSION "/plugins/"),
#else
    // for static builds, we need to add "lib"
    std::string("lib/paraview-" PARAVIEW_VERSION "/plugins/") + plugin,
    std::string("lib/paraview-" PARAVIEW_VERSION "/plugins/"),
#endif

#if defined(__APPLE__)
    // needed for Apps
    std::string("Plugins/") + plugin,
    std::string("Plugins/"),
#elif defined(_WIN32)
    std::string("plugins/") + plugin,
    std::string("plugins/"),
#endif
    std::string()
  };

  const std::string landmark = isPlugin ? vtkGetPluginFileNameFromName(plugin) : plugin;

  vtkNew<vtkPResourceFileLocator> locator;
  locator->SetPrintDebugInformation(debug_plugin);

  // First try the test plugin path, if it exists.
  vtkPVOptions* options = pm->GetOptions();
  if (options && options->GetTestPluginPath() && strlen(options->GetTestPluginPath()) > 0)
  {
    auto path = locator->Locate(options->GetTestPluginPath(), landmark);
    if (!path.empty())
    {
      return path + "/" + landmark;
    }
  }

  // Now, try the prefixes we so careful put together.
  if (!vtklib.empty())
  {
    auto pluginpath =
      locator->Locate(vtksys::SystemTools::GetFilenamePath(vtklib), prefixes, landmark);
    if (!pluginpath.empty())
    {
      return pluginpath + "/" + landmark;
    }
  }
  if (!exe_dir.empty())
  {
    auto pluginpath = locator->Locate(exe_dir, prefixes, landmark);
    if (!pluginpath.empty())
    {
      return pluginpath + "/" + landmark;
    }
  }

  return std::string();
}

/**
 * Converts a filename for a plugin to it's name i.e. removes the library
 * prefix and suffix, if any.
 */
std::string vtkGetPluginNameFromFileName(const std::string& filename)
{
  std::string defaultname = vtksys::SystemTools::GetFilenameWithoutExtension(filename);
  if (defaultname.size() > 3 && VTKSTRNCASECMP(defaultname.c_str(), "lib", 3) == 0)
  {
    defaultname.erase(0, 3);
  }
  return defaultname;
}
}

class vtkPVPluginTracker::vtkPluginsList : public std::vector<vtkItem>
{
public:
  iterator LocateUsingPluginName(const char* pluginname)
  {
    for (iterator iter = this->begin(); iter != this->end(); ++iter)
    {
      if (iter->PluginName == pluginname)
      {
        return iter;
      }
    }
    return this->end();
  }

  iterator LocateUsingFileName(const char* filename)
  {
    for (iterator iter = this->begin(); iter != this->end(); ++iter)
    {
      if (iter->FileName == filename)
      {
        return iter;
      }
    }
    return this->end();
  }
};

vtkPluginSearchFunction vtkPVPluginTracker::StaticPluginSearchFunction = 0;

vtkStandardNewMacro(vtkPVPluginTracker);
//----------------------------------------------------------------------------
vtkPVPluginTracker::vtkPVPluginTracker()
{
  this->PluginsList = new vtkPluginsList();
}

//----------------------------------------------------------------------------
vtkPVPluginTracker::~vtkPVPluginTracker()
{
  delete this->PluginsList;
  this->PluginsList = NULL;
}

//----------------------------------------------------------------------------
vtkPVPluginTracker* vtkPVPluginTracker::GetInstance()
{
  static vtkSmartPointer<vtkPVPluginTracker> Instance;
  if (Instance.GetPointer() == NULL)
  {
    vtkPVPluginTracker* mgr = vtkPVPluginTracker::New();
    Instance = mgr;
    mgr->FastDelete();

    bool debug_plugin = vtksys::SystemTools::GetEnv("PV_PLUGIN_DEBUG") != NULL;
    vtkPVPluginTrackerDebugMacro("Locate and load distributed plugin list.");

    // Locate ".plugins" file and process it.
    // This will setup the distributed-list of plugins. Also it will load any
    // auto-load plugins.
    std::string _plugins =
      vtkLocatePluginOrConfigFile(".plugins", false, StaticPluginSearchFunction);
    if (!_plugins.empty())
    {
      mgr->LoadPluginConfigurationXML(_plugins.c_str());
    }
    else
    {
      vtkPVPluginTrackerDebugMacro("Could not find .plugins file for distributed plugins");
    }
  }

  return Instance;
}

//----------------------------------------------------------------------------
void vtkPVPluginTracker::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//----------------------------------------------------------------------------
void vtkPVPluginTracker::LoadPluginConfigurationXML(const char* filename, bool forceLoad)
{
  bool debug_plugin = vtksys::SystemTools::GetEnv("PV_PLUGIN_DEBUG") != NULL;
  vtkPVPluginTrackerDebugMacro("Loading plugin configuration xml: " << filename);
  if (!vtkPSystemTools::FileExists(filename, true))
  {
    vtkPVPluginTrackerDebugMacro(
      "Failed to located configuration xml. "
      "Could not populate the list of plugins distributed with application.");
    return;
  }

  vtkSmartPointer<vtkPVXMLParser> parser = vtkSmartPointer<vtkPVXMLParser>::New();
  parser->SetFileName(filename);
  parser->SuppressErrorMessagesOn();
  if (!parser->Parse())
  {
    vtkPVPluginTrackerDebugMacro("Configuration file not a valid xml.");
    return;
  }

  this->LoadPluginConfigurationXML(parser->GetRootElement(), forceLoad);
}

//----------------------------------------------------------------------------
void vtkPVPluginTracker::LoadPluginConfigurationXMLFromString(
  const char* xmlcontents, bool forceLoad)
{
  bool debug_plugin = vtksys::SystemTools::GetEnv("PV_PLUGIN_DEBUG") != NULL;
  vtkSmartPointer<vtkPVXMLParser> parser = vtkSmartPointer<vtkPVXMLParser>::New();
  parser->SuppressErrorMessagesOn();
  if (!parser->Parse(xmlcontents))
  {
    vtkPVPluginTrackerDebugMacro("Configuration file not a valid xml.");
    return;
  }

  this->LoadPluginConfigurationXML(parser->GetRootElement(), forceLoad);
}

//----------------------------------------------------------------------------
void vtkPVPluginTracker::LoadPluginConfigurationXML(vtkPVXMLElement* root, bool forceLoad)
{
  if (root == NULL)
  {
    return;
  }

  bool debug_plugin = vtksys::SystemTools::GetEnv("PV_PLUGIN_DEBUG") != NULL;
  if (strcmp(root->GetName(), "Plugins") != 0)
  {
    vtkPVPluginTrackerDebugMacro("Root element in the xml must be <Plugins/>. "
                                 "Got "
      << root->GetName());
    return;
  }

  for (unsigned int cc = 0; cc < root->GetNumberOfNestedElements(); cc++)
  {
    vtkPVXMLElement* child = root->GetNestedElement(cc);
    if (child && child->GetName() && strcmp(child->GetName(), "Plugin") == 0)
    {
      std::string name = child->GetAttributeOrEmpty("name");
      int auto_load = 0;
      child->GetScalarAttribute("auto_load", &auto_load);
      if (name.empty())
      {
        vtkPVPluginTrackerDebugMacro("Missing required attribute name. Skipping element.");
        continue;
      }
      vtkPVPluginTrackerDebugMacro("Trying to locate plugin with name: " << name.c_str());
      std::string plugin_filename;
      if (child->GetAttribute("filename") &&
        vtkPSystemTools::FileExists(child->GetAttribute("filename"), true))
      {
        plugin_filename = child->GetAttribute("filename");
      }
      else
      {
        plugin_filename =
          vtkLocatePluginOrConfigFile(name.c_str(), true, StaticPluginSearchFunction);
      }
      if (plugin_filename.empty())
      {
        int required = 0;
        child->GetScalarAttribute("required", &required);
        if (required)
        {
          vtkErrorMacro("Failed to locate required plugin: "
            << name.c_str() << "\n"
                               "Application may not work exactly as expected.");
        }
        vtkPVPluginTrackerDebugMacro("Failed to locate file plugin: " << name.c_str());
        continue;
      }
      vtkPVPluginTrackerDebugMacro("--- Found " << plugin_filename);
      unsigned int index = this->RegisterAvailablePlugin(plugin_filename.c_str());
      if ((auto_load || forceLoad) && !this->GetPluginLoaded(index))
      {
        // load the plugin.
        vtkPVPluginLoader* loader = vtkPVPluginLoader::New();
        loader->LoadPlugin(plugin_filename.c_str());
        loader->Delete();
      }
      (*this->PluginsList)[index].AutoLoad = (auto_load != 0);
    }
  }
}

//----------------------------------------------------------------------------
unsigned int vtkPVPluginTracker::GetNumberOfPlugins()
{
  return static_cast<unsigned int>(this->PluginsList->size());
}

//----------------------------------------------------------------------------
unsigned int vtkPVPluginTracker::RegisterAvailablePlugin(const char* filename)
{
  std::string defaultname = vtkGetPluginNameFromFileName(filename);
  vtkPluginsList::iterator iter = this->PluginsList->LocateUsingFileName(filename);
  if (iter == this->PluginsList->end())
  {
    iter = this->PluginsList->LocateUsingPluginName(defaultname.c_str());
  }
  if (iter == this->PluginsList->end())
  {
    vtkItem item;
    item.FileName = filename;
    item.PluginName = defaultname;
    this->PluginsList->push_back(item);
    return static_cast<unsigned int>(this->PluginsList->size() - 1);
  }
  else
  {
    // don't update the filename here. This avoids cloberring of paths for
    // distributed plugins between servers that are named the same (as far as
    // the client goes).
    // iter->FileName = filename;
    return static_cast<unsigned int>(iter - this->PluginsList->begin());
  }
}

//----------------------------------------------------------------------------
void vtkPVPluginTracker::RegisterPlugin(vtkPVPlugin* plugin)
{
  assert(plugin != NULL);

  vtkPluginsList::iterator iter = this->PluginsList->LocateUsingPluginName(plugin->GetPluginName());
  if (iter == this->PluginsList->end())
  {
    vtkItem item;
    item.FileName = plugin->GetFileName() ? plugin->GetFileName() : "linked-in";
    item.PluginName = plugin->GetPluginName();
    item.Plugin = plugin;
    this->PluginsList->push_back(item);
  }
  else
  {
    iter->Plugin = plugin;
    if (plugin->GetFileName())
    {
      iter->FileName = plugin->GetFileName();
    }
  }

  // Do some basic processing of the plugin here itself.

  // If this plugin has functions for initializing the interpreter, we set them
  // up right now.
  vtkPVServerManagerPluginInterface* smplugin =
    dynamic_cast<vtkPVServerManagerPluginInterface*>(plugin);
  if (smplugin)
  {
    if (smplugin->GetInitializeInterpreterCallback())
    {
      // This also initializes any existing instances of
      // vtkClientServerInterpreter. Refer to
      // vtkClientServerInterpreterInitializer::RegisterCallback implementation
      // for details.
      vtkClientServerInterpreterInitializer::GetInitializer()->RegisterCallback(
        smplugin->GetInitializeInterpreterCallback());
    }
  }

  // If this plugin has Python modules, process those.
  vtkPVPythonPluginInterface* pythonplugin = dynamic_cast<vtkPVPythonPluginInterface*>(plugin);
  if (pythonplugin)
  {
    std::vector<std::string> modules, sources;
    std::vector<int> package_flags;
    pythonplugin->GetPythonSourceList(modules, sources, package_flags);
    assert(modules.size() == sources.size() && sources.size() == package_flags.size());
    for (size_t cc = 0; cc < modules.size(); cc++)
    {
      vtkPVPythonModule* module = vtkPVPythonModule::New();
      module->SetFullName(modules[cc].c_str());
      module->SetSource(sources[cc].c_str());
      module->SetIsPackage(package_flags[cc]);
      vtkPVPythonModule::RegisterModule(module);
      module->Delete();
    }
  }

  this->InvokeEvent(vtkCommand::RegisterEvent, plugin);
}

//----------------------------------------------------------------------------
vtkPVPlugin* vtkPVPluginTracker::GetPlugin(unsigned int index)
{
  if (index >= this->GetNumberOfPlugins())
  {
    vtkWarningMacro("Invalid index: " << index);
    return NULL;
  }
  return (*this->PluginsList)[index].Plugin;
}

//----------------------------------------------------------------------------
const char* vtkPVPluginTracker::GetPluginName(unsigned int index)
{
  if (index >= this->GetNumberOfPlugins())
  {
    vtkWarningMacro("Invalid index: " << index);
    return NULL;
  }
  return (*this->PluginsList)[index].PluginName.c_str();
}

//----------------------------------------------------------------------------
const char* vtkPVPluginTracker::GetPluginFileName(unsigned int index)
{
  if (index >= this->GetNumberOfPlugins())
  {
    vtkWarningMacro("Invalid index: " << index);
    return NULL;
  }
  return (*this->PluginsList)[index].FileName.c_str();
}

//----------------------------------------------------------------------------
bool vtkPVPluginTracker::GetPluginLoaded(unsigned int index)
{
  if (index >= this->GetNumberOfPlugins())
  {
    vtkWarningMacro("Invalid index: " << index);
    return false;
  }
  return (*this->PluginsList)[index].Plugin != NULL;
}

//----------------------------------------------------------------------------
bool vtkPVPluginTracker::GetPluginAutoLoad(unsigned int index)
{
  if (index >= this->GetNumberOfPlugins())
  {
    vtkWarningMacro("Invalid index: " << index);
    return false;
  }
  return (*this->PluginsList)[index].AutoLoad;
}

//-----------------------------------------------------------------------------
void vtkPVPluginTracker::SetStaticPluginSearchFunction(vtkPluginSearchFunction function)
{
  if (!StaticPluginSearchFunction)
  {
    StaticPluginSearchFunction = function;
  }
}
