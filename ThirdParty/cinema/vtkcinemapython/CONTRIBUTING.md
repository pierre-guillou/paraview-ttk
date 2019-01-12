Contributing to Cinema
======================

This page documents at a very high level how to contribute to Cinema. The
Cinema development cycle is built upon the following components:

1. [Issues][] identify any issues including bugs and feature requests. In
   general, every code change should have an associated issue which identifies
   the bug being fixed or the feature being added.

2. [Merge Requests][] are collections of changes that address issues.

Reporting Issues
================

If you have a bug report or a feature request for ParaView, you can use the
[Issues][] tracker to report a [new issue][].

To report an issue.

1. Register [GitLab Access] to create an account and select a user name.

2. Create a [new issue][].

3. Ensure that the  issue has a **Title** and **Description**
   with enough details for someone from the development team to reproduce the
   issue. See [Gitlab Markdown] guide for styling the **Description**. Include
   screenshots and sample datasets whenever possible.

Fixing issues
=============

Typically, one addresses issues by writing code. To start contributing to a
Cinema project:

1. Register for [GitLab Access] to create an account and select a user name.

2. Fork the relevant repository into your user's namespace on GitLab.

3. Create a local clone of the relevant Cinema repository from your fork.
   Optionally configure Git to [use SSH instead of HTTPS][]. For example if
   you are working on an issue in the `Cinema/cinema_python` repository:

        $ git clone https://gitlab.kitware.com/<username>/cinema_python.git cinema_python

4. Create a topic branch and edit files and create commits to resolve the
   issue (repeat as needed):

        $ edit file1 file2 file3
        $ git add file1 file2 file3
        $ git commit

    Commit messages must be thorough and informative so that reviewers will
    have a good understanding of why the change is needed before looking at the
    code. Appropriately refer to the issue number, if applicable. Cinema
    development uses a [branchy workflow][branchy] equivalent to the one used
    in projects like [VTK][VTK_develop], based on a master branch and different
    topic branches.

    Please make sure your python code follows the [PEP 8] style guidelines.
    The [flake8] tool is a good way to check for violations. If you run the
    `./setup-hooks.sh` after cloning a Cinema repo it will setup pre-commit
    checks with flake8 if you have the tool installed.

5. Depending on the repository you are working with run tests with ctest
   directly or replace the [vtkcinema_python] submodule in [ParaView] with
   your branch and run the Cinema tests in ParaView:

        $ ctest -R Cinema

6. Push your commits in your topic branch to your fork in gitlab.

7. [Create a Merge Request][] and solicit feedback from other Cinema developers.

8. Once another Cinema developer has signed off on your changes then you can
   have the kwrobot merge your changes:

        $ do: merge

[GitLab Access]: https://gitlab.kitware.com/users/sign_in
[Gitlab Markdown]: https://gitlab.kitware.com/help/markdown/markdown
[use SSH instead of HTTPS]: Documentation/dev/git/download.md#use-ssh-instead-of-https
[Issues]: https://gitlab.kitware.com/groups/cinema/issues
[Merge Requests]: https://gitlab.kitware.com/groups/cinema/merge_requests
[new issue]: https://gitlab.kitware.com/groups/cinema/issues/new
[vtkcinema_python]: https://gitlab.kitware.com/paraview/paraview/tree/master/ThirdParty/cinema
[ParaView]: https://gitlab.kitware.com/paraview/paraview
[PEP 8]: https://www.python.org/dev/peps/pep-0008/
[flake8]: http://flake8.pycqa.org/en/latest/#
[Create a Merge Request]: Documentation/dev/git/develop.md#create-a-merge-request
[branchy]: https://gitlab.kitware.com/vtk/vtk/blob/master/Documentation/dev/git/develop.md
[VTK_develop]: https://gitlab.kitware.com/vtk/vtk/blob/master/Documentation/dev/git/develop.md
