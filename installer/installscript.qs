// スタートメニューとデスクトップにショートカットを作る
function Component() {}

Component.prototype.createOperations = function () {
    component.createOperations();
    if (systemInfo.productType === "windows") {
        component.addOperation("CreateShortcut",
            "@TargetDir@/bin/monopoly_gui.exe",
            "@StartMenuDir@/Business Monopoly Recorder.lnk",
            "workingDirectory=@TargetDir@/bin");
        component.addOperation("CreateShortcut",
            "@TargetDir@/bin/monopoly_gui.exe",
            "@DesktopDir@/Business Monopoly Recorder.lnk",
            "workingDirectory=@TargetDir@/bin");
        component.addOperation("CreateShortcut",
            "@TargetDir@/Uninstall.exe",
            "@StartMenuDir@/Uninstall.lnk");
    }
};
