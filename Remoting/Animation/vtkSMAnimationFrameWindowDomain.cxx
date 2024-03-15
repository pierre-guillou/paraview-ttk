// SPDX-FileCopyrightText: Copyright (c) Kitware Inc.
// SPDX-License-Identifier: BSD-3-Clause
#include "vtkSMAnimationFrameWindowDomain.h"

#include "vtkCompositeAnimationPlayer.h"
#include "vtkObjectFactory.h"
#include "vtkSMUncheckedPropertyHelper.h"

vtkStandardNewMacro(vtkSMAnimationFrameWindowDomain);
//----------------------------------------------------------------------------
vtkSMAnimationFrameWindowDomain::vtkSMAnimationFrameWindowDomain() = default;

//----------------------------------------------------------------------------
vtkSMAnimationFrameWindowDomain::~vtkSMAnimationFrameWindowDomain() = default;

//----------------------------------------------------------------------------
void vtkSMAnimationFrameWindowDomain::Update(vtkSMProperty*)
{
  vtkSMProperty* sceneProperty = this->GetRequiredProperty("AnimationScene");
  vtkSMProperty* frameRateProperty = this->GetRequiredProperty("FrameRate");
  if (!sceneProperty)
  {
    vtkErrorMacro("Missing required 'AnimationScene' property.");
    return;
  }
  if (!frameRateProperty)
  {
    vtkErrorMacro("Missing required 'FrameRate' property.");
    return;
  }

  std::vector<vtkEntry> values;
  if (vtkSMProxy* scene = vtkSMUncheckedPropertyHelper(sceneProperty).GetAsProxy())
  {
    int playMode = vtkSMUncheckedPropertyHelper(scene, "PlayMode").GetAsInt();
    switch (playMode)
    {
      case vtkCompositeAnimationPlayer::SEQUENCE:
      {
        int numFrames = vtkSMUncheckedPropertyHelper(scene, "NumberOfFrames").GetAsInt();
        vtkSMProxy* timeKeeper = vtkSMUncheckedPropertyHelper(scene, "TimeKeeper").GetAsProxy();
        if (vtkSMUncheckedPropertyHelper(timeKeeper, "TimestepValues").GetNumberOfElements() == 0)
        {
          numFrames = 1;
        }
        values.push_back(vtkEntry(0, numFrames - 1));
      }
      break;
      case vtkCompositeAnimationPlayer::SNAP_TO_TIMESTEPS:
      {
        vtkSMProxy* timeKeeper = vtkSMUncheckedPropertyHelper(scene, "TimeKeeper").GetAsProxy();
        int numTS =
          vtkSMUncheckedPropertyHelper(timeKeeper, "TimestepValues").GetNumberOfElements();
        values.push_back(vtkEntry(0, numTS - 1));
      }
      break;
    }
  }
  else
  {
    // no scene, nothing much to do.
    values.push_back(vtkEntry(0, 0));
  }
  this->SetEntries(values);
}

//----------------------------------------------------------------------------
void vtkSMAnimationFrameWindowDomain::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}
