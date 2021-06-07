#include "boxedwine.h"
#include "../boxedwineui.h"
#include "../../../lib/imgui/addon/imguitinyfiledialogs.h"

#ifdef BOXEDWINE_OPENGL_OSMESA
bool isMesaOpenglAvailable();
#endif

OptionsView::OptionsView(std::string tab) : BaseView("OptionsView") {
    if (tab.length()) {
        if (tab == "Wine") {
            this->tabIndex = 2;
        } if (tab == "Display") {
            this->tabIndex = 1;
        }

    }
    this->wineTitle = getTranslation(OPTIONSVIEW_TITLE_WINE_VERSISONS);

    createGeneralTab();
    createThemeTab();
    
    std::string name;
    if (GlobalSettings::hasIconsFont()) {
        name += OPTION_WINE_ICON;
        name += " ";
    }
    name += this->wineTitle;
    addTab(name, name, NULL, [this](bool buttonPressed, BaseViewTab& tab) {
        this->runWineOptions();
        });    

    loadWineVersions();
}

void OptionsView::createThemeTab() {
    std::shared_ptr<ImGuiLayout> model = std::make_shared<ImGuiLayout>();
    std::shared_ptr<LayoutSection> section = model->addSection(OPTIONSVIEW_TITLE_DISPLAY);
    
    std::vector<ComboboxItem> themes;
    themes.push_back(ComboboxItem(getTranslation(OPTIONSVIEW_THEME_DARK), "Dark"));
    themes.push_back(ComboboxItem(getTranslation(OPTIONSVIEW_THEME_LIGHT), "Light"));
    themes.push_back(ComboboxItem(getTranslation(OPTIONSVIEW_THEME_CLASSIC), "Classic"));

    themeControl = section->addComboboxRow(OPTIONSVIEW_THEME_LABEL, OPTIONSVIEW_THEME_HELP, themes);
    themeControl->setWidth((int)GlobalSettings::scaleFloatUIAndFont(150));

    themeControl->setSelectionStringValue(GlobalSettings::getTheme());
    themeControl->onChange = [this]() {
        GlobalSettings::setTheme(this->themeControl->getSelectionStringValue());
        GlobalSettings::saveConfig();
    };

    std::vector<ComboboxItem> fontScales;
    fontScales.push_back(ComboboxItem("50%", 50));
    fontScales.push_back(ComboboxItem("75%", 75));
    fontScales.push_back(ComboboxItem("100%", 100));
    fontScales.push_back(ComboboxItem("125%", 125));
    fontScales.push_back(ComboboxItem("150%", 150));
    fontScales.push_back(ComboboxItem("200%", 200));
    std::shared_ptr<LayoutComboboxControl> fontScale = section->addComboboxRow(OPTIONSVIEW_DEFAULT_FONT_SCALE_LABEL, OPTIONSVIEW_DEFAULT_FONT_SCALE_HELP, fontScales);
    fontScale->setWidth((int)GlobalSettings::scaleFloatUIAndFont(150));
    fontScale->setSelectionIntValue((int)(GlobalSettings::fontScale * 100+.5f));
    fontScale->onChange = [fontScale]() {
        runAfterFrame([fontScale]() {
            GlobalSettings::setFontScale(fontScale->getSelectionIntValue() / 100.0f);
            GlobalSettings::saveConfig();
            GlobalSettings::restartUI = true;
            GlobalSettings::startUpArgs.runOnRestartUI = []() {
                gotoView(VIEW_OPTIONS, "Display");
            };
            return false;
        });
    };

    std::string name;
    if (GlobalSettings::hasIconsFont()) {
        name += OPTION_DISPLAY_ICON;
        name += " ";
    }
    name += getTranslation(OPTIONSVIEW_TITLE_DISPLAY);
    addTab(name, name, model, [this](bool buttonPressed, BaseViewTab& tab) {
        });
}

void OptionsView::createGeneralTab() {
    std::shared_ptr<ImGuiLayout> model = std::make_shared<ImGuiLayout>();
    std::shared_ptr<LayoutSection> section = model->addSection(OPTIONSVIEW_TITLE_GENERAL);

    saveLocationControl = section->addTextInputRow(OPTIONSVIEW_SAVE_FOLDER_LABEL, OPTIONSVIEW_SAVE_FOLDER_HELP, GlobalSettings::getDataFolder());
    saveLocationControl->setBrowseDirButton();
    // :TODO: track when we loose focus and save

    std::vector<ComboboxItem> resolutions;
    for (auto& res : GlobalSettings::getAvailableResolutions()) {
        resolutions.push_back(ComboboxItem(res));
    }
    resolutionControl = section->addComboboxRow(OPTIONSVIEW_DEFAULT_RESOLUTION_LABEL, OPTIONSVIEW_DEFAULT_RESOLUTION_HELP, resolutions);
    resolutionControl->setWidth((int)GlobalSettings::scaleFloatUIAndFont(150));
    resolutionControl->setSelectionByLabel(GlobalSettings::getDefaultResolution());
    resolutionControl->onChange = [this]() {
        GlobalSettings::defaultResolution = GlobalSettings::availableResolutions[this->resolutionControl->getSelection()];
        GlobalSettings::saveConfig();
    };

    std::vector<ComboboxItem> vsync;
    vsync.push_back(ComboboxItem("Disabled", VSYNC_DISABLED));
    vsync.push_back(ComboboxItem("Enabled", VSYNC_ENABLED));
    vsync.push_back(ComboboxItem("Adaptive", VSYNC_ADAPTIVE));
    vsyncControl = section->addComboboxRow(OPTIONS_VIEW_VSYNC_LABEL, OPTIONS_VIEW_VSYNC_HELP, vsync, GlobalSettings::defaultVsync);
    vsyncControl->setWidth((int)GlobalSettings::scaleFloatUIAndFont(150));
    vsyncControl->onChange = [this]() {
        GlobalSettings::defaultVsync = this->vsyncControl->getSelectionIntValue();
        GlobalSettings::saveConfig();
    };

    std::vector<ComboboxItem> scales;
    scales.push_back(ComboboxItem("1/2x", 50));
    scales.push_back(ComboboxItem("1x", 100));
    scales.push_back(ComboboxItem("2x", 200));
    scales.push_back(ComboboxItem("3x", 300));
    scaleControl = section->addComboboxRow(OPTIONSVIEW_DEFAULT_SCALE_LABEL, OPTIONSVIEW_DEFAULT_SCALE_HELP, scales, GlobalSettings::getDefaultScale()/100);
    scaleControl->setWidth((int)GlobalSettings::scaleFloatUIAndFont(150));
    scaleControl->onChange = [this]() {
        GlobalSettings::defaultScale = this->scaleControl->getSelectionIntValue();
        GlobalSettings::saveConfig();
    };

#if defined(BOXEDWINE_OPENGL_OSMESA) && defined(BOXEDWINE_OPENGL_SDL)
    if (isMesaOpenglAvailable()) {
        std::vector<ComboboxItem> glOptions;
        glOptions.push_back(ComboboxItem("Native", OPENGL_TYPE_SDL));
        glOptions.push_back(ComboboxItem("Mesa - OpenGL in Software", OPENGL_TYPE_OSMESA));
        openGlControl = section->addComboboxRow(OPTIONSVIEW_DEFAULT_OPENGL_LABEL, OPTIONSVIEW_DEFAULT_OPENGL_HELP, glOptions, GlobalSettings::defaultOpenGL);
        openGlControl->setWidth((int)GlobalSettings::scaleFloatUIAndFont(250));
        openGlControl->setSelectionIntValue(GlobalSettings::defaultOpenGL);
        openGlControl->onChange = [this]() {
            GlobalSettings::defaultOpenGL = this->openGlControl->getSelectionIntValue();
            GlobalSettings::saveConfig();
        };
    }
#endif

    std::shared_ptr<LayoutSection> bottomSection = model->addSection();
    bottomSection->addSeparator();
    std::string deleteLabel = "";
    if (GlobalSettings::hasIconsFont()) {
        deleteLabel += TRASH_ICON;
        deleteLabel += " ";
    }
    deleteLabel += getTranslation(OPTIONS_VIEW_DELETE_ALL_BUTTON_LABEL);
    std::shared_ptr<LayoutButtonControl> deleteAllContainersButton = bottomSection->addButton(0, OPTIONS_VIEW_DELETE_ALL_BUTTON_HELP, deleteLabel);
    deleteAllContainersButton->onChange = [this]() {
        runOnMainUI([this]() {
            new YesNoDlg(GENERIC_DLG_CONFIRM_TITLE, getTranslation(OPTIONS_VIEW_DELETE_ALL_CONFIRM), [this](bool yes) {
                if (yes) {
                    runOnMainUI([this]() {
                        Fs::deleteNativeDirAndAllFilesInDir(GlobalSettings::getDataFolder());
                        GlobalSettings::restartUI = true;
                        GlobalSettings::reinit = true;                      
                        return false;
                        });
                }
                });
            return false;
            });
    };

    std::string name;
    if (GlobalSettings::hasIconsFont()) {
        name += OPTIONS_GENERAL;
        name += " ";
    }
    name += getTranslation(OPTIONSVIEW_TITLE_GENERAL);
    addTab(name, name, model, [this](bool buttonPressed, BaseViewTab& tab) {

        });
}

void OptionsView::loadWineVersions() {
    this->wineButtonTotalColumnWidth = 0;
    this->wineButtonFirstColumnWidth = 0;
    this->wineVersions.clear();

    for (WineVersion& wine : GlobalSettings::availableWineVersions) {
        OptionsViewWineVersion v;
        v.availableVersion = &wine;
        v.name = wine.name;

        U64 size = wine.size;
        WineVersion* dep = wine.getMissingDependency();
        if (dep) {
            size += dep->size;
        }
        v.size = std::to_string(size);
        this->wineVersions[v.name] = v;
    }

    for (WineVersion& wine : GlobalSettings::wineVersions) {
        if (wine.size == 0) {
            wine.size = (U32)(Fs::getNativeFileSize(wine.filePath) / 1024 / 1024);
        }
        if (this->wineVersions.count(wine.name)) {
            OptionsViewWineVersion& v = this->wineVersions[wine.name];
            v.currentVersion = &wine;
        } else {
            OptionsViewWineVersion v;
            v.availableVersion = &wine;
            v.name = wine.name;
            
            U64 size = wine.size;
            WineVersion* dep = wine.getMissingDependency();
            if (dep) {
                size += dep->size;
            }
            v.size = std::to_string(size);
            this->wineVersions[v.name] = v;
        }
    }
    float leftColumn = 0;
    float rightColumn = 0;

    ImGui::PushFont(GlobalSettings::mediumFont);
    for (auto& wine : this->wineVersions) {
        float width = 0.0f;
        if (wine.second.availableVersion && !wine.second.currentVersion) {
            width += ImGui::CalcTextSize(getTranslation(OPTIONSVIEW_WINE_VERSION_INSTALL)).x;            
            width += ImGui::GetStyle().FramePadding.x * 2;
            if (width > leftColumn) {
                leftColumn = width;
            }
        } else if (wine.second.availableVersion && wine.second.currentVersion && wine.second.currentVersion->fsVersion != wine.second.availableVersion->fsVersion) {
            width += ImGui::CalcTextSize(getTranslation(OPTIONSVIEW_WINE_VERSION_UPDATE)).x;
            width += ImGui::GetStyle().FramePadding.x * 2;
            if (width > leftColumn) {
                leftColumn = width;
            }
        }
        if (wine.second.currentVersion) {
            width = ImGui::CalcTextSize(getTranslation(OPTIONSVIEW_WINE_VERSION_DELETE)).x;
            width += ImGui::GetStyle().FramePadding.x * 2;
            if (width > rightColumn) {
                rightColumn = width;
            }
        } 
        if (width > 0) {
            width += ImGui::GetStyle().ItemSpacing.x;
        }
        if (width > this->wineButtonTotalColumnWidth) {
            this->wineButtonTotalColumnWidth = width;
        }
    }
    if (leftColumn > 0 && rightColumn > 0) {
        this->wineButtonFirstColumnWidth = leftColumn + ImGui::GetStyle().ItemSpacing.x;
        this->wineButtonTotalColumnWidth = this->wineButtonFirstColumnWidth + rightColumn + ImGui::GetStyle().ItemSpacing.x;
    } else if (leftColumn > 0) {
        this->wineButtonTotalColumnWidth = leftColumn + ImGui::GetStyle().ItemSpacing.x;
    } else if (rightColumn > 0) {
        this->wineButtonTotalColumnWidth = rightColumn + ImGui::GetStyle().ItemSpacing.x;
    }
    ImGui::PopFont();
}

void OptionsView::runWineOptions() {
    ImGui::PushFont(GlobalSettings::largeFont);
    ImGui::Dummy(ImVec2(0.0f, this->extraVerticalSpacing));
    SAFE_IMGUI_TEXT(this->wineTitle);
    ImGui::PopFont();

    ImVec2 size = ImGui::GetWindowContentRegionMax();
    size.y -= ImGui::GetCursorPosY() + GlobalSettings::scaleFloatUI(8.0f);

    ImGui::PushFont(GlobalSettings::mediumFont);
    //ImGui::BeginChildFrame(401, size);
    ImGui::Dummy(ImVec2(0.0f, this->extraVerticalSpacing));
    for (auto& wine : this->wineVersions) {
        ImGui::Dummy(ImVec2(this->extraVerticalSpacing, 0.0f));        
        ImGui::SameLine();
        ImVec2 pos = ImGui::GetCursorPos();
        pos.y += this->extraVerticalSpacing;
        ImGui::SetCursorPos(pos);
        if (wine.second.availableVersion && !wine.second.currentVersion) {
            std::string buttonLabel = getTranslation(OPTIONSVIEW_WINE_VERSION_INSTALL);
            buttonLabel += "##" + wine.first;
            if (ImGui::Button(buttonLabel.c_str())) {
                download(wine.second.availableVersion);
            }
            ImGui::SameLine();
        } else if (wine.second.availableVersion && wine.second.currentVersion && wine.second.currentVersion->fsVersion!=wine.second.availableVersion->fsVersion){
            std::string buttonLabel = getTranslation(OPTIONSVIEW_WINE_VERSION_UPDATE);
            buttonLabel += "##" + wine.first;
            if (ImGui::Button(buttonLabel.c_str())) {
                download(wine.second.availableVersion);
            }
            ImGui::SameLine();
        }
        if (wine.second.currentVersion) {
            ImGui::SameLine(pos.x + this->wineButtonFirstColumnWidth);
            bool buttonPressed = false;
            std::string buttonLabel = getTranslation(OPTIONSVIEW_WINE_VERSION_DELETE);
            buttonLabel += "##" + wine.first;
            if (ImGui::Button(buttonLabel.c_str())) {
                buttonPressed = true;
            }
            bool yes = false;
            std::string label = getTranslationWithFormat(OPTIONSVIEW_WINE_VERSION_DELETE_CONFIRM_LABEL, true, wine.first);
            if (!showYesNoMessageBox("confirm"+wine.first, buttonPressed, getTranslation(GENERIC_DLG_CONFIRM_TITLE), label.c_str(), &yes)) {
                if (yes) {
                    Fs::deleteNativeFile(wine.second.currentVersion->filePath);
                    // run later, we don't want to change versions while we are iterating them
                    runOnMainUI([this]()->bool {
                        GlobalSettings::reloadWineVersions();
                        this->loadWineVersions();
                        return false;
                        });
                }
            }
            ImGui::SameLine();
        }     
        ImGui::SameLine(pos.x+this->wineButtonTotalColumnWidth);
        std::string name = wine.first;
        std::string name2;
        OptionsViewWineVersion& version = wine.second;

        if (version.currentVersion) {
            if (version.availableVersion && version.availableVersion->fsVersion==version.currentVersion->fsVersion) {
                name2 += "   ";
                name2 += getTranslation(OPTIONSVIEW_WINE_VERSION_UPTODATE);
                name2 += version.size;
                name2 += " MB";
            } else {
                name2 += "   ";
                name2 += getTranslation(OPTIONSVIEW_WINE_VERSION_UPDATE_AVAILABLE);
                name2 += version.size;
                name2 += " MB";
            }
        } else if (version.availableVersion) {
            name2 += "   ";
            name2 += getTranslation(OPTIONSVIEW_WINE_VERSION_NOT_INSTALLED);
            name2 += version.size;
            name2 += " MB";
        }
        SAFE_IMGUI_TEXT(name.c_str());
        ImGui::SameLine();
        ImGui::PushFont(GlobalSettings::defaultFont);
        pos = ImGui::GetCursorPos();
        if (GlobalSettings::mediumFont) {
            pos.y += GlobalSettings::mediumFont->FontSize / 2 - GlobalSettings::defaultFont->FontSize / 2;
        }
        ImGui::SetCursorPos(pos);
        SAFE_IMGUI_TEXT(name2.c_str());
        ImGui::PopFont();
        pos = ImGui::GetCursorPos();
        pos.y += this->extraVerticalSpacing;
        ImGui::SetCursorPos(pos);
    }
    ImGui::PopFont();
    //ImGui::EndChildFrame();
}

void OptionsView::download(WineVersion* version) {
    GlobalSettings::downloadWine(*version, [this](bool success) {
        if (success) {
            runOnMainUI([this]()->bool {                
                this->loadWineVersions();
                return false;
                });
        }
        });
}

bool OptionsView::saveChanges() {
    this->errorMsg = NULL;
    if (!Fs::doesNativePathExist(this->saveLocationControl->getText())) {
        this->errorMsg = getTranslation(OPTIONSVIEW_ERROR_DATA_DIR_NOT_FOUND);
    }
    if (!this->errorMsg) {
        GlobalSettings::setDataFolder(this->saveLocationControl->getText());
        GlobalSettings::saveConfig();
    }

    if (this->errorMsg) {
        runErrorMsg(true);
    }
    return this->errorMsg == NULL;
}