# AndroidFlurryPlugin
Android Flurry Analytics Provider plugin for UE4

# Installation

1. Enable UE4's Analytics Blueprint Library as shown here: https://docs.unrealengine.com/latest/INT/Gameplay/Analytics/Blueprints/index.html
2. Either clone, checkout or unzip the contents of this repo to a folder called "AndroidFlurryPlugin" inside your project's Plugin folder.
2. If you haven't already, go to https://dev.flurry.com/ create an account, add an application and get your API key in the app info page (in the "Manage" category).
3. Edit your project's **Config/Android/AndroidEngine.ini** (create it if needed) so it contains the snipped below:

    ```INI
    [Analytics]
    ProviderModuleName=AndroidFlurry
    FlurryApiKey=YOUR_KEY_HERE
    ```
4. Replace *YOUR_KEY_HERE* by your key.
5. Done! Now use the blueprint Analytics API as you would in iOS.

# Notes

- Session tracking is done automatically without need for BeginSession and EndSession calls. You still to make those calls in order to keep compatibility with the UE4 iOS Flurry plugin, of course.
