This folder provides a location for sample scripts which can be used either via unattended setup boot scripts or front panel scripts on an Xbox One DevKit equipped with a front panel display.

For unattended setup boot scripts, place the script at the root of a USB drive or copy it to d:\boot\boot.cmd on the console.
For front panel display scripts, copy the script to d:\QuickActions on the console.

For either script type, you can load the script in the Automation tab of Windows Device Portal running on an Xbox One Devkit.

Sample scripts may require you to load them either via Windows Device Portal or via a text editor in order to populate information needed for the script. eg network share names or credentials, account names, etc.

Scripts should be self-documenting when possible to make it clear which needs to be populated and what the script is doing.

More documentation can be found via the XDK docs under Development Environment->Overviews->Advanced XDK and Console Setup->Configuring Your Dev Kit with an Unattended Setup Script

This readme should also be updated to point to whitepaper samples when those become available.
