# WSL Tux System Tray Monitor

This is a Windows 10/11 system tray app to monitor and control your Windows Subsystem for Linux distributions. It gives a visual indication that a WSL distribution is active (or that none are active).

Usage:
- run the app, click on a distribution you want to run, and click Start
- close the app with the X in the corner, or click on cancel, and the app will stay running in the background
- the system tray icon of Tux will be in color if at least one distribution is running, otherwise Tux will be faded and grayed out
- click on Tux in the system tray to bring back the main dialog window
- right click on the Tux icon and click Exit to terminate the application
- double click the Tux icon to launch an interactive session of your default distribution

Current packages will install this app as a startup application (with the --start-minimized option), so that you can always have a Tux icon in your system tray so that you know if you have Linux running or not, and can double click it to quickly launch an interactive terminal window of your default distribution. It is also recommended to turn it on as an icon which appears in your taskbar corner, and not hidden in the overflow (Taskbar settings -> Taskbar corner overflow).
