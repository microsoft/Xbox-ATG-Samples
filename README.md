# Xbox-ATG-Samples

This repo contains game development samples written by the Microsoft Xbox Advanced Technology Group.

* ``Kits`` contains support code used by the samples
* ``Media`` contains media files used by the samples
* ``UWPSamples`` contains samples for the Universal Windows Platform
  * ``Audio``
  * ``IntroGraphics``
  * ``Graphics``
  * ``System``
  * ``Tools``
* ``PCSamples`` contains samples for the classic Win32 desktop PC platform
  * ``IntroGraphics``

# Requirements

## UWP apps
* Windows 10 Creators Update (Build 15063)
* Visual Studio 2017 with the *Universal Windows Platform development* workload, the *C++ Universal Windows Platform tools* component, *Windows 10 SDK (10.0.15063.0)*, and *Windows 10 SDK (10.0.14393.0)*.

> The *Windows 10 SDK (10.0.14393.0)* is required to support ``WindowsTargetPlatformMinVersion`` set the Windows 10 Anniversary Update in many of the samples that work on both versions of Windows 10.

## PC apps
* Visual Studio 2015 Update 3 -or- Visual Studio 2017 (_via upgrade in place_) with the *Desktop development with C++* workload and *Windows 8.1 SDK* component.
* _DirectX 11:_ Windows 7 Service Pack 1 with the DirectX 11.1 Runtime via [KB2670838](http://support.microsoft.com/kb/2670838) or later.
* _DirectX 12:_ Windows 10; requires the Windows 10 Anniversary Update SDK (14393) or later to build.

# Privacy Statement

When compiling and running a sample, the file name of the sample executable will be sent to Microsoft to help track sample usage. To opt-out of this data collection, you can remove the block of code in ``Main.cpp`` labeled _Sample Usage Telemetry_.

For more information about Microsoft’s privacy policies in general, see the [Microsoft Privacy Statement](https://privacy.microsoft.com/en-us/privacystatement/).

# Code of Conduct

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/). For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.
