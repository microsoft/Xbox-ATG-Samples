# Xbox-ATG-Samples

**These samples are for the older Xbox One XDK. For the latest Xbox ATG samples using the Microsoft GDK, see [Xbox-GDK-Samples](https://github.com/microsoft/Xbox-GDK-Samples/).**

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
  * ``Graphics``
  * ``IntroGraphics``
* ``XDKSamples`` contains samples for the Xbox One platform using the Xbox One XDK
  * ``Audio``
  * ``IntroGraphics``
  * ``Graphics``
  * ``System``
  * ``Tools``

## Samples by category

### Audio

<table>
 <tr>
  <td>Spatial audio</td>
  <td><a href="UWPSamples/Audio/SimpleSpatialPlaySoundUWP">Simple playback UWP</a></td>
  <td><a href="XDKSamples/Audio/SimpleSpatialPlaySoundXDK">Simple playback XDK</a></td>
  <td><a href="UWPSamples/Audio/SimplePlay3DSpatialSoundUWP">3D playback UWP</a></td>
  <td><a href="UWPSamples/Audio/AdvancedSpatialSoundsUWP">Advanced audio UWP</a></td>
  <td><a href="XDKSamples/Audio/AdvancedSpatialSoundsXDK">Advanced audio XDK</a></td>
 </tr>
</table>

<table>
 <tr>
  <td>XAudio2: Basic audio</td>
  <td><a href="UWPSamples/Audio/SimplePlaySoundUWP">UWP</a></td>
  <td><a href="XDKSamples/Audio/SimplePlaySound">XDK</a></td>
 </tr>
 <tr>
  <td>XAudio2: Streaming</td>
  <td><a href="UWPSamples/Audio/SimplePlaySoundStreamUWP">UWP</a></td>
  <td><a href="XDKSamples/Audio/SimplePlaySoundStream">XDK</a></td>
 </tr>
 <tr>
  <td>XAudio2: 3D playback</td>
  <td><a href="UWPSamples/Audio/SimplePlay3DSoundUWP">UWP</a></td>
  <td><a href="XDKSamples/Audio/SimplePlay3DSoundXDK">XDK</a></td>
 </tr>
</table>

<table>
 <tr>
  <td>WASAPI: Playback</td>
  <td><a href="UWPSamples/Audio/SimpleWASAPIPlaySoundUWP">UWP</a></td>
  <td><a href="XDKSamples/Audio/SimpleWASAPIPlaySoundXDK">XDK</a></td>
 </tr>
 <tr>
  <td>WASAPI: Capture</td>
  <td><a href="UWPSamples/Audio/SimpleWASAPICaptureUWP">UWP</a></td>
  <td><a href="XDKSamples/Audio/SimpleWASAPICaptureXDK">XDK</a></td>
 </tr>
</table>

<table>
 <tr>
  <td>Play Text to Speech</td>
  <td><a href="XDKSamples/Audio/SimplePlayTextToSpeechXDK">XDK</a></td>
 </tr>
</table>

### Introductory Graphics

<table>
 <tr>
  <td>Basic drawing</td>
  <td><a href="UWPSamples/IntroGraphics/SimpleTriangleUWP">UWP DX11</a></td>
  <td><a href="UWPSamples/IntroGraphics/SimpleTriangleUWP12">UWP DX12</a></td>
  <td><a href="XDKSamples/IntroGraphics/SimpleTriangle">XDK DX11</a></td>
  <td><a href="XDKSamples/IntroGraphics/SimpleTriangle12">XDK DX12</a></td>
  <td><a href="PCSamples/IntroGraphics/SimpleTrianglePC">PC DX11</a></td>
  <td><a href="PCSamples/IntroGraphics/SimpleTrianglePC12">PC DX12</a></td>
 </tr>
 <tr>
  <td></td>
  <td><a href="UWPSamples/IntroGraphics/SimpleTriangleCppWinRT_UWP">UWP (C++/WinRT) DX11</a></td>
  <td><a href="UWPSamples/IntroGraphics/SimpleTriangleCppWinRT_UWP12">UWP (C++/WinRT) DX12</a></td>
  <td><a href="XDKSamples/IntroGraphics/SimpleTriangleCppWinRT">XDK (C++/WinRT) DX11</a></td>
  <td><a href="XDKSamples/IntroGraphics/SimpleTriangleCppWinRT12">XDK (C++/WinRT) DX12</a></td>
 </tr>
 <tr>
  <td>Basic texturing</td>
  <td><a href="UWPSamples/IntroGraphics/SimpleTextureUWP">UWP DX11</a></td>
  <td><a href="UWPSamples/IntroGraphics/SimpleTextureUWP12">UWP DX12</a></td>
  <td><a href="XDKSamples/IntroGraphics/SimpleTexture">XDK DX11</a></td>
  <td><a href="XDKSamples/IntroGraphics/SimpleTexture12">XDK DX12</a></td>
  <td><a href="PCSamples/IntroGraphics/SimpleTexturePC">PC DX11</a></td>
  <td><a href="PCSamples/IntroGraphics/SimpleTexturePC12">PC DX12</a></td>
 </tr>
 <tr>
  <td>Basic lighting</td>
  <td><a href="UWPSamples/IntroGraphics/SimpleLightingUWP">UWP DX11</a></td>
  <td><a href="UWPSamples/IntroGraphics/SimpleLightingUWP12">UWP DX12</a></td>
  <td><a href="XDKSamples/IntroGraphics/SimpleLighting">XDK DX11</a></td>
  <td><a href="XDKSamples/IntroGraphics/SimpleLighting12">XDK DX12</a></td>
  <td><a href="PCSamples/IntroGraphics/SimpleLightingPC">PC DX11</a></td>
  <td><a href="PCSamples/IntroGraphics/SimpleLightingPC12">PC DX12</a></td>
 </tr>
 <tr>
  <td>Bezier</td>
  <td><a href="UWPSamples/IntroGraphics/SimpleBezierUWP">UWP DX11</a></td>
  <td><a href="UWPSamples/IntroGraphics/SimpleBezierUWP12">UWP DX12</a></td>
  <td><a href="XDKSamples/IntroGraphics/SimpleBezier">XDK DX11</a></td>
  <td><a href="XDKSamples/IntroGraphics/SimpleBezier12">XDK DX12</a></td>
  <td><a href="PCSamples/IntroGraphics/SimpleBezierPC">PC DX11</a></td>
  <td><a href="PCSamples/IntroGraphics/SimpleBezierPC12">PC DX12</a></td>
 </tr>
 <tr>
  <td>DirectCompute</td>
  <td><a href="UWPSamples/IntroGraphics/SimpleComputeUWP">UWP DX11</a></td>
  <td><a href="UWPSamples/IntroGraphics/SimpleComputeUWP12">UWP DX12</a></td>
  <td><a href="XDKSamples/IntroGraphics/SimpleCompute">XDK DX11</a></td>
  <td><a href="XDKSamples/IntroGraphics/SimpleCompute12">XDK DX12</a></td>
  <td><a href="PCSamples/IntroGraphics/SimpleComputePC">PC DX11</a></td>
  <td><a href="PCSamples/IntroGraphics/SimpleComputePC12">PC DX12</a></td>
 </tr>
 <tr>
  <td>DirectX Tool Kit</td>
  <td><a href="UWPSamples/IntroGraphics/DirectXTKSimpleSampleUWP">UWP DX11</a></td>
  <td><a href="UWPSamples/IntroGraphics/DirectXTKSimpleSampleUWP12">UWP DX12</a></td>
  <td><a href="XDKSamples/IntroGraphics/DirectXTKSimpleSample">XDK DX11</a></td>
  <td><a href="XDKSamples/IntroGraphics/DirectXTKSimpleSample12">XDK DX12</a></td>
  <td><a href="PCSamples/IntroGraphics/DirectXTKSimpleSamplePC">PC DX11</a></td>
  <td><a href="PCSamples/IntroGraphics/DirectXTKSimpleSamplePC12">PC DX12</a></td>
 </tr>
 <tr>
  <td>Instancing</td>
  <td><a href="UWPSamples/IntroGraphics/SimpleInstancingUWP">UWP DX11</a></td>
  <td><a href="UWPSamples/IntroGraphics/SimpleInstancingUWP12">UWP DX12</a></td>
  <td><a href="XDKSamples/IntroGraphics/SimpleInstancing">XDK DX11</a></td>
  <td><a href="XDKSamples/IntroGraphics/SimpleInstancing12">XDK DX12</a></td>
  <td><a href="PCSamples/IntroGraphics/SimpleInstancingPC">PC DX11</a></td>
  <td><a href="PCSamples/IntroGraphics/SimpleInstancingPC12">PC DX12</a></td>
 </tr>
 <tr>
  <td>Multisample Antialiasing</td>
  <td><a href="UWPSamples/IntroGraphics/SimpleMSAA_UWP">UWP DX11</a></td>
  <td><a href="UWPSamples/IntroGraphics/SimpleMSAA_UWP12">UWP DX12</a></td>
  <td><a href="XDKSamples/IntroGraphics/SimpleMSAA">XDK DX11</a></td>
  <td><a href="XDKSamples/IntroGraphics/SimpleMSAA12">XDK DX12</a></td>
  <td><a href="PCSamples/IntroGraphics/SimpleMSAA_PC">PC DX11</a></td>
  <td><a href="PCSamples/IntroGraphics/SimpleMSAA_PC12">PC DX12</a></td>
 </tr>
 <tr>
  <td>Xbox One Device Setup</td>
  <td><a href="XDKSamples/IntroGraphics/SimpleDeviceAndSwapChain">XDK DX11</a></td>
  <td><a href="XDKSamples/IntroGraphics/SimpleDeviceAndSwapChain12">XDK DX12</a></td>
 </tr>
</table>

### Graphics

<table>
 <tr>
  <td>Physically Based Rendering</td>
  <td><a href="UWPSamples/Graphics/SimplePBR12_UWP">UWP</a></td>
  <td><a href="XDKSamples/Graphics/SimplePBR12_Xbox">XDK</a></td>
 </tr>
 <tr>
  <td>High-Dynamic Range Rendering</td>
  <td><a href="UWPSamples/Graphics/SimpleHDR_UWP">UWP DX11</a></td>
  <td><a href="UWPSamples/Graphics/SimpleHDR_UWP12">UWP DX12</a></td>
  <td><a href="XDKSamples/Graphics/SimpleHDR">XDK DX11</a></td>
  <td><a href="XDKSamples/Graphics/SimpleHDR12">XDK DX12</a></td>
  <td><a href="PCSamples/Graphics/SimpleHDR_PC">PC DX11</a></td>
  <td><a href="PCSamples/Graphics/SimpleHDR_PC12">PC DX12</a></td>
 </tr>
</table>

<table>
 <tr>
  <td>Bokeh Effect</td>
  <td><a href="XDKSamples/Graphics/Bokeh">XDK DX11</a></td>
  <td><a href="XDKSamples/Graphics/Bokeh12">XDK DX12</a></td>
 </tr>
</table>

<table>
 <tr>
  <td>Fast Block Compress</td>
  <td><a href="XDKSamples/Graphics/FastBlockCompress">XDK</a></td>
 </tr>
</table>

<table>
 <tr>
  <td>Media Foundation</td>
  <td><a href="PCSamples/Graphics/VideoTexturePC12">PC DX12</a></td>
  <td><a href="UWPSamples/Graphics/VideoTextureUWP">UWP DX11</a></td>
  <td><a href="UWPSamples/Graphics/VideoTextureUWP12">UWP DX12</a></td>
  <td><a href="XDKSamples/Graphics/MP4Reader">XDK</a></td>
 </tr>
</table>

<table>
<tr>
  <td>ESRAM (XDK only)</td>
  <td><a href="XDKSamples/Graphics/SimpleESRAM">Simple DX11</a></td>
  <td><a href="XDKSamples/Graphics/SimpleESRAM12">Simple DX12</a></td>
  <td><a href="XDKSamples/Graphics/AdvancedESRAM12">Advanced DX12</a></td>
 </tr>
</table>

<table>
 <tr>
  <td>Xbox One (XDK only)</td>
  <td><a href="XDKSamples/Graphics/AsyncPresent">AsyncPresent</a></td>
  <td><a href="XDKSamples/Graphics/HLSLSymbols">HLSL Symbols</a></td>
  <td><a href="XDKSamples/Graphics/SimpleDmaDecompression">Simple DMA Decompression</a></td>
 </tr>
</table>

### System

<table>
 <tr>
  <td>UWP</td>
  <td><a href="UWPSamples/System/CPUSets">CPU Sets</a></td>
  <td><a href="UWPSamples/System/MemoryStatisticsUWP">Memory Statistics</a></td>
 </tr>
 <tr>
  <td>XDK</td>
  <td><a href="XDKSamples/System/AsynchronousIO">Async I/O</a></td>
  <td><a href="XDKSamples/System/CoroutinesXDK">Coroutines</a>
  <td><a href="XDKSamples/System/CustomEventProvider">Custom Event Provider</a></td>
  <td><a href="XDKSamples/System/DataBreakpoints">Data Breakpoints</a></td>
  <td><a href="XDKSamples/System/GameDVR">Game DVR</a></td>
  <td><a href="XDKSamples/System/MemoryBanks">Memory Banks</a></td>
  <td><a href="XDKSamples/System/UserManagement">User Management</a></td>
 </tr>
</table>

<table>
 <tr>
  <td>System Information</td>
  <td><a href="UWPSamples/System/SystemInfoUWP">UWP</a></td>
  <td><a href="XDKSamples/System/SystemInfo">XDK</a></td>
 </tr>
 <tr>
  <td>DirectXMath</td>
  <td><a href="UWPSamples/System/CollisionUWP">UWP</a></td>
  <td><a href="XDKSamples/System/Collision">XDK</a></td>
 </tr>
</table>

<table>
 <tr>
  <td>Gamepad</td>
  <td><a href="UWPSamples/System/GamepadUWP">UWP</a></td>
  <td><a href="XDKSamples/System/Gamepad">XDK</a></td>
  <td><a href="UWPSamples/System/GamepadCppWinRT_UWP">UWP (C++/WinRT)</a></td>
  <td><a href="XDKSamples/System/GamepadCppWinRT">XDK (C++/WinRT)</a></td>
 </tr>
 <tr>
  <td>Gamepad Vibration</td>
  <td><a href="UWPSamples/System/GamepadVibrationUWP">UWP</a></td>
  <td><a href="XDKSamples/System/GamepadVibration">XDK</a></td>
 </tr>
 <tr>
  <td>Raw Game Controller</td>
  <td><a href="UWPSamples/System/RawGameControllerUWP">UWP</a></td>
 </tr>
 <tr>
  <td>ArcadeStick</td>
  <td><a href="XDKSamples/System/ArcadeStick">XDK</a></td>
 </tr>
 <tr>
  <td>FlightStick</td>
  <td><a href="XDKSamples/System/FlightStick">XDK</a></td>
 </tr>
 <tr>
  <td>WheelTest</td>
  <td><a href="XDKSamples/System/WheelTest">XDK</a></td>
 </tr>
 <tr>
  <td>Input</td>
  <td><a href="UWPSamples/System/InputInterfacingUWP">Interfacing UWP</a></td>
  <td><a href="UWPSamples/System/MouseCursor">Mouse UWP</a></td>
  <td><a href="XDKSamples/System/MouseCursorXDK">Mouse XDK</a></td>
 </tr>
 <tr>
  <td>User Gamepad Pairing</td>
  <td><a href="UWPSamples/System/UserGamepadPairingUWP">UWP</a></td>
  <td><a href="XDKSamples/System/UserGamepadPairing">XDK</a></td>
 </tr>
</table>

<table>
 <tr>
  <td>Process Lifetime Management</td>
  <td><a href="UWPSamples/System/SimplePLM_UWP">UWP PLM</a></td>
  <td><a href="XDKSamples/System/SimplePLM">XDK PLM</a></td>
  <td><a href="UWPSamples/System/ExtendedExecutionUWP">Extended Execution</a></td>
 </tr>
</table>

<table>
 <tr>
  <td>Localization</td>
  <td><a href="UWPSamples/System/NLSAndLocalizationUWP">UWP</a></td>
  <td><a href="XDKSamples/System/NLSAndLocalization">XDK</a></td>
 </tr>
</table>

<table>
 <tr>
  <td>Device RGB Lamp Array</td>
  <td><a href="UWPSamples/System/LampArrayUWP">UWP</a></td>
  <td><a href="XDKSamples/System/LampArrayXDK">XDK</a></td>
 </tr>
</table>

<table>
 <tr>
  <td>Xbox One X Front Panel</td>
  <td><a href="XDKSamples/System/SimpleFrontPanel">Basic</a></td>
  <td><a href="XDKSamples/System/FrontPanelText">Text</a></td>
  <td><a href="XDKSamples/System/FrontPanelDemo">Demo</a></td>
  <td><a href="XDKSamples/System/FrontPanelDolphin">Dolphin</a></td>
  <td><a href="XDKSamples/System/FrontPanelGame">Game</a></td>
  <td><a href="XDKSamples/System/FrontPanelLogo">Logo</a></td>
 </tr>
</table>

### Tools

<table>
 <tr>
  <td><a href="UWPSamples/Tools/errorlookup/errorlookup">Error lookup</a></td>
 </tr>
</table>

<table>
 <tr>
  <td><a href="XDKSamples\Tools\DumpTool">CrashDump</a></td>
  <td><a href="XDKSamples\Tools\SymbolProxyClient">Symbol Proxy</a></td>
  <td><a href="XDKSamples\Tools\OSPrimitiveTool">OS Primitive</a></td>
  <td><a href="XDKSamples\Tools\xtexconv">TexConv for Xbox</a></td>
 </tr>
</table>

<table>
 <tr>
  <td><a href="XDKSamples\Tools\RasterFontGen">FrontPanel Font</a></td>
  <td><a href="XDKSamples\Tools\RasterFontViewer">FrontPanel Font Viewer</a></td>
 </tr>
</table>

### DirectX Raytracing (DXR)

<table>
 <tr>
  <td><a href="PCSamples/Raytracing/SimpleRaytracingTriangle_PC12">SimpleTriangle</a></td>
  <td><a href="PCSamples/Raytracing/SimpleRaytracingInstancing_PC12">SimpleInstancing</a></td>
  <td><a href="PCSamples/Raytracing/RaytracingAO_PC12">Raytracing AO</a></td>
 </tr>
</table>

## Requirements

### UWP apps
* Windows 10 Anniversary Update (Version 1607) or later
* Visual Studio 2017 ([15.9](https://walbourn.github.io/vs-2017-15-9-update/) update) with the *Universal Windows Platform development* workload, the *C++ Universal Windows Platform tools* component, and *Windows 10 SDK ([10.0.19041.0](https://walbourn.github.io/windows-10-may-2020-update-sdk/))*.

### XDK apps
* Xbox One Development Kit
* Xbox One XDK April 2018 - July 2018.
* Visual Studio 2017 (15.9)

### PC apps
* Visual Studio 2017 (15.9) with the *Desktop development with C++* workload and *Windows 10 SDK ([10.0.19041.0](https://walbourn.github.io/windows-10-may-2020-update-sdk/))*.
* _DirectX 11:_ Windows 7 Service Pack 1 with the DirectX 11.1 Runtime via [KB2670838](http://support.microsoft.com/kb/2670838) or later.
* _DirectX 12:_ Windows 10 or Windows 11.
* _DirectX Raytracing:_ Windows 10 October 2018 Update or later.

### VS 2019 / VS 2022

VS 2019 or VS 2022 can also be used with the required workload and components installed via *Retarget solution* for PC and UWP samples. The legacy Xbox One XDK does not support VS 2019 or later.

## Privacy Statement

When compiling and running a sample, the file name of the sample executable will be sent to Microsoft to help track sample usage. To opt-out of this data collection, you can remove the block of code in ``Main.cpp`` labeled _Sample Usage Telemetry_.

For more information about Microsoft's privacy policies in general, see the [Microsoft Privacy Statement](https://privacy.microsoft.com/privacystatement/).

## Code of Conduct

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/). For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.

## Trademarks

This project may contain trademarks or logos for projects, products, or services. Authorized use of Microsoft trademarks or logos is subject to and must follow [Microsoft's Trademark & Brand Guidelines](https://www.microsoft.com/en-us/legal/intellectualproperty/trademarks/usage/general). Use of Microsoft trademarks or logos in modified versions of this project must not cause confusion or imply Microsoft sponsorship. Any use of third-party trademarks or logos are subject to those third-party's policies.

## Other Samples

For more ATG samples, see [DirectML-Samples](https://github.com/microsoft/DirectML-Samples), [PlayFab-Samples](https://github.com/PlayFab/PlayFab-Samples), and [Xbox-LIVE-Samples](https://github.com/microsoft/xbox-live-samples).
