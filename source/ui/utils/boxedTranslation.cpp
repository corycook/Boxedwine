#include "boxedwine.h"
#include "boxedTranslation.h"
#include <stdarg.h>
const std::string& translateString(const std::string& s) {
    return s;
}

std::string getTranslationWithFormat(int msg, bool useDefaultIfMissing, const std::string& string1) {
    return getTranslationWithFormat(msg, useDefaultIfMissing, std::vector<std::string>(1, string1));
}

std::string getTranslationWithFormat(int msg, bool useDefaultIfMissing, const std::string& string1, const std::string& string2) {
    std::vector<std::string> values;
    values.push_back(string1);
    values.push_back(string2);
    return getTranslationWithFormat(msg, useDefaultIfMissing, values);
}

std::string getTranslationWithFormat(int msg, bool useDefaultIfMissing, const std::vector<std::string>& replacements) {
    std::string result = getTranslation(msg, useDefaultIfMissing);
    for (int i=0;i<(int)replacements.size();i++) {
        std::string findText = "{"+std::to_string(i)+"}";
        stringReplaceAll(result, findText, replacements[i]);
    }
    return result;
}

const char* getTranslation(int msg, bool useDefaultIfMissing) {
    switch (msg) {
    case INSTALLVIEW_INSTALL_TITLE:
        return "Install";
    case INSTALLVIEW_DEMO_TITLE:
        return "Demos";
    case INSTALLVIEW_CONTAINER_HELP:
        return "A Container is a directory on then file system where this application will installed into.  In general, is is best that each application have its own container, that way if you need to change anything in the container, like what version of Windows to emulate, then it will only affect this one application";
    case INSTALLVIEW_CONTAINER_NAME_HELP:
        return "This is the name of the container that you will see in the UI.";
    case INSTALLVIEW_TYPE_SETUP_HELP:
        return "This setup program will be run from here and it will be installed into a container";
    case INSTALLVIEW_TYPE_DIR_HELP:
        return "The contents of this directory will be copied from your system into a container";
    case INSTALLVIEW_TYPE_MOUNT_HELP:
        return "This folder on your system will be used by Boxedwine without copying it to a container.";    
    case INSTALLVIEW_RUN_WINE_CONFIG_HELP:
        return "Wine Config can be used to change the Windows version that is currently being emulated by Wine.  For example if your installer or application fails to run because it says it needs Windows 95, the you can use Wine Config to fix this.";
    case INSTALLVIEW_INSTALL_TYPE_HELP:
        return NULL;
    case INSTALLVIEW_INSTALL_TYPE_LABEL:
        return "Install Type:";
    case INSTALLVIEW_CONTAINER_LABEL:
        return "Container:";
    case INSTALLVIEW_CONTAINER_NAME_LABEL:
        return "Name:";    
    case INSTALLVIEW_CONTAINER_RUN_WINE_CONFIG_LABEL:
        return "Run Wine Config:";
    case INSTALLVIEW_SETUP_FILE_LOCATION_LABEL:
        return "Setup File Location:";
    case INSTALLVIEW_DIRECTORY_LABEL:
        return "Directory:";
    case INSTALLVIEW_TITLE:
        return "Install Application";
    case INSTALLVIEW_ERROR_SETUP_FILE_MISSING:
        return "Setup file location is empty and is required.";
    case INSTALLVIEW_ERROR_SETUP_FILE_NOT_FOUND:
        return "The setup file was entered, but it does not exist.";
    case INSTALLVIEW_ERROR_SETUP_FILE_NOT_FILE:
        return "The setup file path is a directory, this should be the location of a setup file.";
    case INSTALLVIEW_ERROR_DIR_MISSING:
        return "Directory location is empty and is required.";
    case INSTALLVIEW_ERROR_DIR_NOT_FOUND:
        return "The directory was entered, but it does not exist.";
    case INSTALLVIEW_ERROR_CONTAINER_NAME_MISSING:
        return "You must enter a name for your new container.";
    case INSTALLVIEW_ERROR_FAILED_TO_CREATE_CONTAINER_DIR:
        return "Failed to create a directory for the new container:\n\nerror msg: {0}";
    case INSTALLVIEW_ERROR_FAILED_TO_CREATE_TEMP_DIR:
        return "Failed to create temp directory:\n\n{0}\n\nerror msg: {1}";
    case INSTALLVIEW_TYPE_SETUP:
        return "Install using a setup program";
    case INSTALLVIEW_TYPE_MOUNT:
        return "Install by mounting a directory";
    case INSTALLVIEW_TYPE_BLANK:
        return "Create a blank container";
    case INSTALLVIEW_TYPE_DIRECTORY:
        return "Install by copying a directory";
    case INSTALLVIEW_ERROR_FILESYSTEM_COPY_DIRECTORY:
        return "The file system reported an error while trying to copy the directory:\n\n{0}";
    case INSTALLVIEW_ERROR_FILESYSTEM_FAIL_TO_CREATE_DIRS:
        return "The file system reported an error while trying to create the directory:\n\n{0}";
    case INSTALLVIEW_ERROR_FAILED_TO_MOUNT:
        return "Failed to mount directory at drive t:.  If this is not a new container, perhaps you can try the same thing with a new container.";
    case INSTALLVIEW_INSTALL_BUTTON_LABEL:
        return "Install";
    case INSTALLVIEW_NEW_CONTAINER:
        return "Create New Container (Recommended)";
    case INSTALLVIEW_DEMO_DOWNLOAD_SIZE:
        return "Download size: ";
    case SETTINGS_DLG_TITLE:
        return "Options";
    case OPTIONSVIEW_SAVE_FOLDER_LABEL:
        return "Save Folder Location:";
    case OPTIONSVIEW_SAVE_FOLDER_HELP:
        return "This is the location on your computer where the games and apps will be installed.  If you already installed some games or apps, if you change this you will no longer see them.";
    case OPTIONSVIEW_THEME_LABEL:
        return "Color Theme:";
    case OPTIONSVIEW_THEME_HELP:
        return "If you prefer Boxedwine to use different colors for the windows, you can try changing this.";
    case OPTIONSVIEW_THEME_DARK:
        return "Dark";
    case OPTIONSVIEW_THEME_CLASSIC:
        return "Classic";
    case OPTIONSVIEW_THEME_LIGHT:
        return "Light";
    case OPTIONSVIEW_ERROR_DATA_DIR_NOT_FOUND:
        return "The Save Folder Location does not exist.";
    case OPTIONSVIEW_TITLE_GENERAL:
        return "General";
    case OPTIONSVIEW_TITLE_DISPLAY:
        return "Display";
    case OPTIONSVIEW_TITLE_WINE_VERSISONS:
        return "Wine Versions";
    case OPTIONSVIEW_WINE_VERSION_UPTODATE:
        return "Up to date, size: ";
    case OPTIONSVIEW_WINE_VERSION_UPDATE_AVAILABLE:
        return "Update available, download size: ";
    case OPTIONSVIEW_WINE_VERSION_NOT_INSTALLED:
        return "Not installed, download size: ";
    case OPTIONSVIEW_WINE_VERSION_DELETE:
        return "Delete";
    case OPTIONSVIEW_WINE_VERSION_INSTALL:
        return "Install";
    case OPTIONSVIEW_WINE_VERSION_UPDATE:
        return "Update";    
    case OPTIONSVIEW_WINE_VERSION_DELETE_CONFIRM_LABEL:
        return "Are you sure you want to delete {0}?";
    case OPTIONSVIEW_DEFAULT_RESOLUTION_LABEL:
        return "Default App Resolution:";
    case OPTIONSVIEW_DEFAULT_RESOLUTION_HELP:
        return "This is the default resolution that Boxedwine will use to create the main window.  This can be overriden in the Containers view under the shortcut specific options on a per app basis.";
    case OPTIONSVIEW_DEFAULT_SCALE_LABEL:
        return "Default App Scale:";
    case OPTIONSVIEW_DEFAULT_SCALE_HELP:
        return "This is the default scale that Boxedwine will use to create the main window.  This can be overriden in the Containers view under the shortcut specific options on a per app basis.\n\nThis simple scaling option will blow up or shrink the emualted screen.  This is useful for applications that want to run in low resolution, like 640x480, and you want it to show larger on your monitor.";
    case OPTIONSVIEW_DEFAULT_OPENGL_LABEL:
        return "Default OpenGL Backend:";
    case OPTIONSVIEW_DEFAULT_OPENGL_HELP:
        return "OpenGL Backends:\n\nNative - This will use the OpenGL that is installed on your computer for your graphics card.  This is the best option if it works for you and will probably be the fastest.\n\nThe other options available to choose are in the order of expected performance.  They might be help older games/apps that try to use a color depth no longer supported by modern OpenGL graphics cards.";
    case OPTIONSVIEW_DEFAULT_FONT_SCALE_LABEL:
        return "Font Scale";
    case OPTIONSVIEW_DEFAULT_FONT_SCALE_HELP:
        return "This will change the size of the, which will make the UI look smaller or bigger.";
    case OPTIONS_VIEW_DELETE_ALL_BUTTON_LABEL:
        return "Delete all data";
    case OPTIONS_VIEW_DELETE_ALL_BUTTON_HELP:
        return "This will delete all data, including the containers folder, and cannot be undone.  You will loose all saved data inside the containers, including apps, and Boxedwine settings.";
    case OPTIONS_VIEW_DELETE_ALL_CONFIRM:
        return "Are you sure you want to permentantly delete all containers, apps, settings and any saved data you had in the containers?";
    case OPTIONS_VIEW_VSYNC_LABEL:
        return "Default App Vsync:";
    case OPTIONS_VIEW_VSYNC_HELP:
        return "Disabled: The frame rate can be faster than the monitor, but may introduce artifacts in the game.\n\nEnabled: Synchronizes the frame rate of the app/game with the monitor refresh rate for better stability.\n\nAdaptive: At high framerates, VSync is enabled to eliminate tearing. At low frame rates, it's disabled to minimise stuttering.  If the video card does not support this, then this option will act like Enabled.";
    case HELPVIEW_TITLE_ABOUT:
        return "About";
    case HELPVIEW_TITLE_HELP_INSTALL:
        return "Installing Apps";
    case HELPVIEW_TITLE_HELP_TROUBLESHOOTING:
        return "Troubleshooting";
    case HELPVIEW_ABOUT_LABEL:
        return "## What is Boxedwine?\n[Boxedwine](http://boxedwine.org) is a multi-platform app for running 16-bit and 32-bit Windows programs.  It does this by emulating the CPU and running [Wine](https://www.winehq.org/).  Wine is the program that emulates all of the Windows API calls.  The goal of Boxedwine is to be able to run everything that Wine can.  If you want to know if your app or game will run in Boxedwine or get hints how to run it, you should start by looking at the [Wine Application Database (AppDB)](https://appdb.winehq.org/)\n## Supported Platforms:\n* Windows 32-bit (Vista or later)\n* Windows 64-bit (Vista or later)\n* Mac OSX\n* Linux (32-bit and 64-bit)\n* Web Browsers using WASM\n* Raspberry Pi (32-bit and 64-bit)\n## Current Version: " BOXEDWINE_VERSION_DISPLAY "\n* Added command line option: -fullscreenAspect  This will enable full screen with letter boxing in order to preserve the aspect ratio.\n* Added command line option : -vsync X where X is 0, 1 or 2.  0 is disabled, 1 is enabled and 2 is adaptive.The default is 0 (disabled).\n* Added command line option : -showWindowImmediately  This fixes simple OpenGL GLUT examples.\n* OpenGL : Implemented glInterleavedArray, fixed glFeedbackBufferand fixed glReadPixels when using packed formats.\n* Recompiled Wine : will now use SSE for floating point math.\n* Rewrote the audio driver, now winealsa.drv.so will talk to Boxedwine directly.\n* Added an implementation for ddraw.dll ddraw7_WaitForVerticalBlank.\n* Lots of work on the Mac build.\n* Windows build will use DirectX 9 for UI instead of OpenGL";
    case HELPVIEW_HELP_INSTALL_LABEL:
        return "Apps/Games are installed into containers.  A container is a folder that contains all of the files necessary for Boxedwine and Wine to run and a place for them to store changes.  It acts as a virtual file system.  Each app can be installed into its own container.  To install an app or game, just drag the installer, for example, setup.exe, onto Boxedwine.  You can also drag a folder onto Boxedwine and it will be copied into a container.";
    case HELPVIEW_HELP_TROUBLESHOOTING_LABEL:
        return "The first place to look for help is the [Wine Application Database (AppDB)](https://appdb.winehq.org/).  If your app or game works on Wine, then there is a good chance it will work on Boxedwine.\n\n## Performance\nSince Boxedwine emulates the CPU, you may have performance issues.  The 64-bit Windows build uses a binary translator for the CPU emulation and is pretty fast, perhaps as much as 25% of the host computer.  The other platforms will be significantly slower.  You will probably not have much luck with demanding games after 1999.\n## Things to try if your app/game doesnt' work\n* If your app/game was made before the year 2000, try setting the Windows version for the container to be Windows 98.";
    case APPCHOOSER_DLG_TITLE:
        return "Create Shortcut";
    case APPCHOOSER_DLG_CHOOSE_APP_LABEL:
        return "Please select a file to use for the shortcut.";
    case APPCHOOSER_DLG_NO_APPS_LABEL:
        return "Could not find any new apps.";
    case APPCHOOSER_DLG_WINE_APPS_LABEL:
        return "Wine Apps:";
    case CONTAINER_VIEW_CONTAINER_NAME_LABEL:
        return "Container Name:";
    case CONTAINER_VIEW_ERROR_BLANK_NAME:
        return "Container Name can not be empty";
    case CONTAINER_VIEW_ERROR_MISSING_MOUNT_LOCATION:
        return "You specified a mount drive letter, but the mount folder location is empty.";
    case CONTAINER_VIEW_ERROR_MISSING_MOUNT_DRIVE:
        return "You specified a mount folder location, but the mount drive letter is empty.";
    case CONTAINER_VIEW_CONTAINER_LOCATION_LABEL:
        return "Storage Location:";
    case CONTAINER_VIEW_CONTAINER_LOCATION_SIZE_LABEL:
        return "Storage Size:";
    case CONTAINER_VIEW_DELETE_BUTTON_LABEL:
        return "Delete Container and its Apps";
    case CONTAINER_VIEW_DELETE_CONFIRMATION:
        return "Are you sure you want to delete the container {0}?";
    case CONTAINER_VIEW_DELETE_CONFIRMATION_WITH_APPS:
        return "Are you sure you want to delete the container {0} and the following associated apps: {1}?";
    case CONTAINER_VIEW_DELETE_BUTTON_HELP:
        return "Deleting a container will delete its data folder.  All apps associated with the container will also be deleted.  This can not be undone.";
    case CONTAINER_VIEW_MOUNT_DIR_LABEL:
        return "Mount Folder:";
    case CONTAINER_VIEW_MOUNT_DIR_HELP:
        return "Instead of copying data to the container folder, you can just mount a folder on your computer and use it in Wine as a drive";
    case CONTAINER_VIEW_SHORTCUT_LIST_LABEL:
        return "Shortcuts:";
    case CONTAINER_OPTIONS_DLG_TITLE:
        return "Container Options";
    case CONTAINER_OPTIONS_DLG_WINE_VERSION_LABEL:
        return "Wine Version:";
    case CONTAINER_OPTIONS_DLG_ADD_APP_LABEL:
        return "New Shortcut:";
    case CONTAINER_OPTIONS_DLG_ADD_APP_HELP:
        return "You can choose from a list of applications in this container to create a shortcut to.";
    case CONTAINER_OPTIONS_DLG_ADD_APP_BUTTON_LABEL:
        return "Choose App";
    case COMMON_WINE_VERSION_LABEL:
        return "Wine Version:";
    case COMMON_WINE_VERSION_HELP:
        return "You should just use the default Wine version, which is the most recent version, unless you know your application needs another version.";
    case UNZIP_DLG_TITLE:
        return "Unzipping ...";
    case GENERIC_BROWSE_BUTTON:
        return "Browse";
    case GENERIC_OPEN_BUTTON:
        return "Open";
    case GENERIC_DLG_OK:
        return "Ok";
    case GENERIC_DLG_YES:
        return "Yes";
    case GENERIC_DLG_NO:
        return "No";
    case GENERIC_DLG_CANCEL:
        return "Cancel";
    case GENERIC_DLG_ERROR_TITLE:
        return "Error";
    case GENERIC_DLG_CONFIRM_TITLE:
        return "Confirm";
    case GENERIC_DEFAULT:
        return "Default";
    case GENERIC_COMBOBOX_ALL:
        return "All";
    case MAIN_BUTTON_INSTALL:
        return "Install";
    case MAIN_BUTTON_CONTAINERS:
        return "Containers";
    case MAIN_BUTTON_SETTINGS:
        return "Options";
    case MAIN_BUTTON_HELP:
        return "Help";
    case MAIN_BUTTON_APPS:
        return "Apps";
    case INSTALLVIEW_OPEN_SETUP_FILE_TITLE:
        return "Open File";
    case GENERIC_OPEN_FOLDER_TITLE:
        return "Select Folder";
    case WAITDLG_LAUNCH_APP_TITLE:
        return "Please Wait";
    case WAITDLG_LAUNCH_APP_LABEL:
        return "Launching {0} ...";
    case WAITDLG_GET_FILE_LIST_TITLE:
        return "Please Wait";
    case WAITDLG_GET_FILE_LIST_LABEL:
        return "Fetching list of Wine versions";
    case DOWNLOADDLG_TITLE:
        return "Please Wait";
    case DOWNLOADDLG_LABEL:
        return "Downloading {0} ...";
    case DOWNLOADDLG_CANCELLING_LABEL:
        return "Trying to cancel download ...";
    case CONTAINER_VIEW_TITLE:
        return "Application Options";
    case CONTAINER_VIEW_NAME_LABEL:
        return "Name:";
    case CONTAINER_VIEW_NAME_HELP:
        return "This is the name you will see in the Apps list view";
    case CONTAINER_VIEW_RESOLUTION_LABEL:
        return "Resolution:";
    case CONTAINER_VIEW_RESOLUTION_HELP:
        return "This is the desktop/monitor resolution that will be used for the initial window and reported back to the application.  Normally the application will set its own resolution if it goes full screen.  This would only need to be adjusted if you want more space for applications that do not go full screen.";
    case CONTAINER_VIEW_BPP_LABEL:
        return "Bits per Pixel:";
    case CONTAINER_VIEW_BPP_HELP:
        return "Some applications might check the screen BPP before starting.  If those applications require a specific BPP, you can set it here.";        
    case CONTAINER_VIEW_FULL_SCREEN_LABEL:
        return "Fullscreen:";
    case CONTAINER_VIEW_FULL_SCREEN_HELP:
        return "Will launch the emulated desktop with the resolution selected and scale it to fit the screen.  This will not work for OpenGL so if your game uses OpenGL or Direct3D then this will have no effect.  If you are using a DirectDraw game you can click DD GDI Renderer above to force it not to use OpenGL.";
    case CONTAINER_VIEW_VSYNC_LABEL:
        return "Vsync:";
    case CONTAINER_VIEW_VSYNC_HELP:
        return "Not Set: Will use the value from the BoxedWine Options\\General screen.\n\nDisabled: The frame rate can be faster than the monitor, but may introduce artifacts in the game.\n\nEnabled: Synchronizes the frame rate of the app/game with the monitor refresh rate for better stability.\n\nAdaptive: At high framerates, VSync is enabled to eliminate tearing. At low frame rates, it's disabled to minimise stuttering.  If the video card does not support this, then this option will act like Enabled.";
    case CONTAINER_VIEW_DPI_AWARE_LABEL:
        return "DPI Aware:";
    case CONTAINER_VIEW_DPI_AWARE_HELP:
        return "DPI (dots per inch) Aware means the app will be responsible for itself when it comes to scaling to large resolutions.  If this is checked and the app can not handle it, then the app might appear to run in a window that is too small.";
    case CONTAINER_VIEW_SHOW_WINDOW_LABEL:
        return "Show Window Immediatly:";
    case CONTAINER_VIEW_SHOW_WINDOW_HELP:
        return "By default Boxedwine will hide new Windows until it looks like they will be used.  This is done to prevent a lot of Window flashing (create and destroy) when games test the system for what resolution and capabilities they will use.  Some simple OpenGL apps seem to have a problem with this feature of Boxedwine so this flag will disable it by showing the windows as soon as it is created.";
    case CONTAINER_VIEW_SCALE_LABEL:
        return "Scaling:";
    case CONTAINER_VIEW_SCALE_HELP:
        return "This simple scaling option will blow up or shrink the emualted screen.  This is useful for applications that want to run in low resolution, like 640x480, and you want it to show larger on your monitor.\n\nScaling is disabled if full screen is selected because full screen will scale the emulated resolution to fill the screen.";
    case CONTAINER_VIEW_SCALE_QUALITY_LABEL:
        return "Scale Quality:";
    case CONTAINER_VIEW_SCALE_QUALITY_HELP:
        return "The scale quality options that SDL supplies.  Probably no need to ever change the default.";
    case CONTAINER_VIEW_GL_EXT_LABEL:
        return "OpenGL Extensions:";
    case CONTAINER_VIEW_GL_EXT_HELP:
        return "If empty then all extension will be allowed.\n\nSome applications, like Quake 2 and Unreal, can crash if the extension list is too large, like what is seen with modern computers.  Hit the set button to fill in a small list of common extensions or enter your own if necessary";
    case CONTAINER_VIEW_GL_EXT_SET_BUTTON_LABEL:
        return "Set";
    case CONTAINER_VIEW_DEFAULT_RESOLUTION_LABEL:
        return "Default";
    case CONTAINER_VIEW_NAME_REQUIRED:
        return "The shorcut name is required and cannot be empty.";
    case CONTAINER_VIEW_SHORTCUT_PATH_LABEL:
        return "Path:";
    case CONTAINER_VIEW_SHORTCUT_PATH_HELP:
        return "This is the path in the container that will be launched when starting this app.";
    case CONTAINER_VIEW_SHORTCUT_ARGUMENTS_LABEL:
        return "Arguments:";
    case CONTAINER_VIEW_SHORTCUT_ARGUMENTS_HELP:
        return "These arguments will be passed to the app.  Place each argument on its own line.";
    case CONTAINER_VIEW_DELETE_SHORTCUT:
        return "Delete Shortcut";
    case CONTAINER_VIEW_SHORTCUT_LIST_HELP:
        return "Shortcuts are links to apps inside a container.  They will appear in the app list and clicking them in the app list will launch the app with the settings you set here.";
    case CONTAINER_VIEW_DELETE_SHORTCUT_CONFIRMATION:
        return "Are you sure you want to the {0} shortcut?";
    case CONTAINER_VIEW_GDI_RENDERER_LABEL:
        return "DD GDI Renderer:";
    case CONTAINER_VIEW_PROGRAMS_LABEL:
        return "Run in Container:";
    case CONTAINER_VIEW_COMPONENTS_LABEL:
        return "Components:";
    case CONTAINER_VIEW_REGEDIT_BUTTON_LABEL:
        return "Regedit";
    case CONTAINER_VIEW_WINECFG_BUTTON_LABEL:
        return "WineCfg";
    case CONTAINER_VIEW_GDI_RENDERER_HELP:
        return "DirectDraw will use OpenGL by default.  But a few games, like Diable and Startcraft, may have graphical issues.  Sometimes changing the renderer to GDI can help.";
    case CONTAINER_VIEW_MOUSE_WARP_LABEL:
        return "Mouse Warp Override:";
    case CONTAINER_VIEW_MOUSE_WARP_HELP:
        return "Enable:  (default) warp pointer when mouse exclusively acquired\nDisable : never warp the mouse pointer\nForce : always warp the pointer";
    case CONTAINER_VIEW_WINDOWS_VERION_LABEL:
        return "Windows Version:";
    case CONTAINER_VIEW_WINDOWS_VERION_HELP:
        return "This is the Windows version that will be reported to the running app.  You probably never need to change this unless the app asks for a specific version of Windows.";
    case CONTAINER_VIEW_CPU_AFFINITY_LABEL:
        return "CPU Affinity:";
    case CONTAINER_VIEW_CPU_AFFINITY_HELP:
        return "This is the number of host CPUs the app can use.  This should almost always be set to All.  But sometimes there will be an older game, around the late 90s, that requires this to be set to 1";
    case CONTAINER_VIEW_SELECT_WINE_APP_LABEL:
        return "Select which Wine app you would like to run in this container.";
    case CONTAINER_VIEW_SELECT_WINE_APP_DLG_TITLE:
        return "Run Wine App";
    case CONTAINER_VIEW_RUNE_WINE_APP_BUTTON_LABEL:
        return "Run Wine App";
    case CONTAINER_VIEW_POLL_RATE_LABEL:
        return "Poll Rate:";
    case CONTAINER_VIEW_POLL_RATE_HELP:
        return "Poll Rate is how often a mouse or keyboard event will be given to Wine.  If the event happens before allowed, the event will be paused or in the case of a mouse move event, dropped.  Some games will experience lockups/hangs when moving the mouse quickly with a high DPI mouse.  Lowering this number can help with that.  A value of 0 means there will be no delay.";
    case CONTAINER_VIEW_SKIP_FRAMES_LABEL:
        return "Skip Frames:";
    case CONTAINER_VIEW_SKIP_FRAMES_HELP:
            return "Some games draw directly to the screen/window instead of using DirectX or OpenGL.  There is no reason to update to the screen more than the refresh rate of the monitor, so if a game updates the screen/window by drawing small updates each time, this could result in things like 900 updates per seconds, which would make Boxedwine update the monitor at 900 FPS.  This is a waste of CPU and could slow down the emulation.\n\nIf your app/game is running slow and it doesn't use DirectX or OpenGL, you could try changing this value to 30 or 60.";
    case ERROR_NO_WINE:
        return "There are no versions of Wine installed.  Would you like to install the default version of Wine now?";
    case ERROR_NO_WINE_HIGH_DPI:
        return "There are no versions of Wine installed.  Would you like to install the default version of Wine now?  Installing a version of Wine will also fix the small fonts you are seeing.";
    case ERROR_MISSING_WINE:
        return "{0} is missing.  Would you like to use {1} instead?";
    case ERROR_NO_FILE_LIST:
        return "Was unable to download the list of Wine versions.  Boxedwine will not work without a Wine file system.  Make sure your internet is working and you can try again by re-launching Boxedwine.  You can also manually download a Wine file system from https://sourceforge.net/projects/boxedwine/files/FileSystems/ and place it in the same directory as the Boxedwine application";
    case WINE_UPGRADE_AVAILABLE_TITLE:
        return "Upgrade Wine?";
    case WINE_UPGRADE_AVAILABLE_LABEL:
        return "Would you like to upgrade Wine?  Upgrades are available for: {0}";
    default:
        if (useDefaultIfMissing) {
            return "Unknown msg";
        }
        return NULL;
    }
}
