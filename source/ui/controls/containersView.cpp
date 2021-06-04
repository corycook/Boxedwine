#include "boxedwine.h"
#include "../boxedwineui.h"
#include "../../../lib/imgui/addon/imguitinyfiledialogs.h"

#ifdef BOXEDWINE_OPENGL_OSMESA
bool isMesaOpenglAvailable();
#endif

ContainersView::ContainersView(std::string tab, std::string app) : BaseView("ContainersView"), currentContainer(NULL), currentContainerChanged(false), currentContainerMountChanged(false), currentApp(NULL), currentAppChanged(false) {
    std::shared_ptr<ImGuiLayout> model = std::make_shared<ImGuiLayout>();        
    section = model->addSection();

    containerNameControl = section->addTextInputRow(CONTAINER_VIEW_CONTAINER_NAME_LABEL, 0);
    containerNameControl->onChange = [this]() {
        this->setTabName(this->tabIndex, this->containerNameControl->getText());
        currentContainer->setName(this->containerNameControl->getText());
        section->setTitle(currentContainer->getName());
        this->currentContainerChanged = true;
    };

    containerWineVersionControl = createWineVersionCombobox(section);
    containerWineVersionControl->onChange = [this]() {        
        this->currentContainerChanged = true;
    };

    containerWindowsVersionControl = createWindowsVersionCombobox(section);
    containerWindowsVersionControl->onChange = [this]() {        
        this->currentContainerChanged = true;
    };


    containerGdiControl = section->addCheckbox(CONTAINER_VIEW_GDI_RENDERER_LABEL, CONTAINER_VIEW_GDI_RENDERER_HELP, false);
    containerGdiControl->onChange = [this]() {        
        this->currentContainerChanged = true;
    };

    std::vector<ComboboxItem> mouseOptions;
    mouseOptions.push_back(ComboboxItem("Enable", "enable"));
    mouseOptions.push_back(ComboboxItem("Disable", "disable"));
    mouseOptions.push_back(ComboboxItem("Force", "force"));
    containerMouseWarpControl = section->addComboboxRow(CONTAINER_VIEW_MOUSE_WARP_LABEL, CONTAINER_VIEW_MOUSE_WARP_HELP, mouseOptions);    
    containerMouseWarpControl->onChange = [this]() {
        this->currentContainerChanged = true;
    };
    containerMouseWarpControl->setWidth((int)GlobalSettings::scaleFloatUIAndFont(150));

    std::vector<ComboboxItem> mountDrives;
    mountDrives.push_back(ComboboxItem(" "));
    for (int i = 3; i < 26; i++) {
        mountDrives.push_back(ComboboxItem(std::string(1, (char)('A' + i)) + ":", std::string(1, (char)('a' + i))));
    }
    std::shared_ptr<LayoutRow> row = section->addRow(CONTAINER_VIEW_MOUNT_DIR_LABEL, CONTAINER_VIEW_MOUNT_DIR_HELP);
    containerMountDriveControl = row->addComboBox(mountDrives);
    containerMountDriveControl->setWidth((int)GlobalSettings::scaleFloatUIAndFont(50));
    containerMountDriveControl->onChange = [this]() {
        this->currentContainerChanged = true;
        this->currentContainerMountChanged = true;
    };
    containerMountPathControl = row->addTextInput();
    containerMountPathControl->setBrowseDirButton();
    containerMountPathControl->onChange = [this]() {
        this->currentContainerChanged = true;
        this->currentContainerMountChanged = true;
    };

    row = section->addRow(CONTAINER_VIEW_CONTAINER_LOCATION_LABEL, 0);
    containerLocationControl = row->addTextInput("", true);
    std::shared_ptr<LayoutButtonControl> showButtonControl = row->addButton(getTranslation(GENERIC_OPEN_BUTTON));
    showButtonControl->onChange = [this]() {
        Platform::openFileLocation(currentContainer->getDir());
    };

    std::shared_ptr<LayoutButtonControl> selectWineAppButton = section->addButton(CONTAINER_VIEW_PROGRAMS_LABEL, 0, getTranslation(CONTAINER_VIEW_RUNE_WINE_APP_BUTTON_LABEL));
    selectWineAppButton->onChange = [this]() {
        if (saveChanges()) { // need to capture any changes to mount
            runOnMainUI([this] {
                std::vector<BoxedApp> wineApps;
                this->currentContainer->getWineApps(wineApps);
                AppChooserDlg* dlg = new AppChooserDlg(wineApps, [this](BoxedApp app) {
                    std::vector<std::string> args;
                    args.push_back("/bin/wine");
                    args.push_back(app.getCmd());
                    this->currentContainer->launch(args, app.getCmd());
                    GlobalSettings::startUpArgs.title = app.getName();
                    GlobalSettings::startUpArgs.setWorkingDir(app.getPath());
                    std::string containerPath = this->currentContainer->getDir();
                    GlobalSettings::startUpArgs.runOnRestartUI = [containerPath]() {
                        gotoView(VIEW_CONTAINERS, containerPath);
                    };
                    }, false, NULL, CONTAINER_VIEW_SELECT_WINE_APP_DLG_TITLE);
                dlg->setLabelId(CONTAINER_VIEW_SELECT_WINE_APP_LABEL);
                return false;
                });
        }
    };

    if (GlobalSettings::getComponents().size()) {
        row = section->addRow(CONTAINER_VIEW_COMPONENTS_LABEL, 0);

        std::vector<ComboboxItem> components;
        for (auto& component : GlobalSettings::getComponents()) {
            components.push_back(ComboboxItem(component.name));
        }
        componentsControl = row->addComboBox(components, 0);

        std::shared_ptr<LayoutButtonControl> installButton = row->addButton(getTranslation(INSTALLVIEW_INSTALL_BUTTON_LABEL));
        installButton->onChange = [this]() {
            if (this->saveChanges()) {
                AppFile& app = GlobalSettings::getComponents()[componentsControl->getSelection()];
                if (Fs::doesNativePathExist(app.localFilePath)) {
                    app.install(false, this->currentContainer);
                } else {
                    GlobalSettings::downloadFile(app.filePath, app.localFilePath, app.name, app.size, [&app, this](bool sucess) {
                        app.install(false, this->currentContainer);
                        });
                }
            }
        };
    }

    section->addSeparator();
    std::shared_ptr<LayoutButtonControl> selectAppButton = section->addButton(CONTAINER_OPTIONS_DLG_ADD_APP_LABEL, CONTAINER_OPTIONS_DLG_ADD_APP_HELP, getTranslation(CONTAINER_OPTIONS_DLG_ADD_APP_BUTTON_LABEL));
    selectAppButton->onChange = [this]() {
        if (saveChanges()) { // need to capture any changes to mount
            runOnMainUI([this] {
                std::vector<BoxedApp> items;
                std::vector<BoxedApp> wineApps;
                this->currentContainer->getNewApps(items);
                this->currentContainer->getWineApps(wineApps);
                new AppChooserDlg(items, wineApps, [this](BoxedApp app) {
                    std::string iniPath = app.getIniFilePath();
                    this->setCurrentApp(app.getContainer()->getAppByIniFile(iniPath));
                    rebuildShortcutsCombobox();
                    showAppSection(true);
                    this->appPickerControl->setSelectionStringValue(iniPath);
                    });
                return false;
                });
        }
    };

    appSection = model->addSection();
    appSection->setIndent(true);

    row = section->addRow(CONTAINER_VIEW_SHORTCUT_LIST_LABEL, CONTAINER_VIEW_SHORTCUT_LIST_HELP);
    appPickerControl = row->addComboBox();
    appPickerControl->onChange = [this]() {
        if (this->saveChanges()) {
            this->setCurrentApp(this->currentContainer->getApps()[this->appPickerControl->getSelection()]);
        }
    };

    std::string label;
    /*
    if (GlobalSettings::hasIconsFont()) {
        label += DELETE_ICON;
        label += " ";
    }
    */
    label += getTranslation(CONTAINER_VIEW_DELETE_SHORTCUT);
    std::shared_ptr<LayoutButtonControl> deleteButton = row->addButton(label);
    deleteButton->onChange = [this]() {
        std::string label = getTranslationWithFormat(CONTAINER_VIEW_DELETE_SHORTCUT_CONFIRMATION, true, this->currentApp->getName());
        runOnMainUI([label, this]() {
            new YesNoDlg(GENERIC_DLG_CONFIRM_TITLE, label, [this](bool yes) {
                if (yes) {
                    runOnMainUI([this]() {
                        this->currentAppChanged = false;
                        this->currentApp->remove();
                        if (this->currentContainer->getApps().size()) {
                            this->setCurrentApp(this->currentContainer->getApps()[0]);
                        } else {
                            this->currentApp = NULL;
                        }
                        showAppSection(this->currentContainer->getApps().size() != 0);
                        rebuildShortcutsCombobox();
                        GlobalSettings::reloadApps();
                        return false;
                        });
                }
                });
            return false;
            });
    };
    row = appSection->addRow(0, 0);
    row->setTopMargin(0.0f);
    row->addCustomControl([this]() {
        if (this->currentApp && this->currentApp->getIconTexture()) {
            ImVec2 pos = ImGui::GetCursorPos();
            ImGui::SetCursorPosX(0);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + GlobalSettings::extraVerticalSpacing);
            ImGui::Image(this->currentApp->getIconTexture()->texture->getTexture(), ImVec2((float)UiSettings::ICON_SIZE, (float)UiSettings::ICON_SIZE));
            ImGui::SetCursorPos(pos);
        }
        });

    appNameControl = appSection->addTextInputRow(CONTAINER_VIEW_NAME_LABEL, CONTAINER_VIEW_NAME_HELP);
    appNameControl->onChange = [this]() {
        this->currentApp->name = appNameControl->getText();
        this->currentAppChanged = true;
        rebuildShortcutsCombobox();
    };

    appPathControl = appSection->addTextInputRow(CONTAINER_VIEW_SHORTCUT_PATH_LABEL, CONTAINER_VIEW_SHORTCUT_PATH_HELP, "", true);
    appArgumentsControl = appSection->addTextInputRow(CONTAINER_VIEW_SHORTCUT_ARGUMENTS_LABEL, CONTAINER_VIEW_SHORTCUT_ARGUMENTS_HELP);
    appArgumentsControl->setNumberOfLines(1);
    appArgumentsControl->onChange = [this]() {
        this->currentAppChanged = true;
        std::vector<std::string> args;
        stringSplit(args, appArgumentsControl->getText(), '\n');
        appArgumentsControl->setNumberOfLines((int)args.size() + 1);
    };

    std::vector<ComboboxItem> resolutions;
    resolutions.push_back(ComboboxItem(getTranslation(GENERIC_DEFAULT), ""));
    for (auto& res : GlobalSettings::getAvailableResolutions()) {
        resolutions.push_back(ComboboxItem(res));
    }
    appResolutionControl = appSection->addComboboxRow(CONTAINER_VIEW_RESOLUTION_LABEL, CONTAINER_VIEW_RESOLUTION_HELP, resolutions);
    appResolutionControl->onChange = [this]() {
        this->currentAppChanged = true;
    };

    std::vector<ComboboxItem> bpp;
    bpp.push_back(ComboboxItem("32-bit (default)", 32));
    bpp.push_back(ComboboxItem("16-bit", 16));
    bpp.push_back(ComboboxItem("8-bit (256 colors)", 8));
    appBppControl = appSection->addComboboxRow(CONTAINER_VIEW_BPP_LABEL, CONTAINER_VIEW_BPP_HELP, bpp);
    appBppControl->onChange = [this]() {
        this->currentAppChanged = true;
    };

    std::vector<ComboboxItem> fullscreen;
    fullscreen.push_back(ComboboxItem("Not Set", FULLSCREEN_NOTSET));
    fullscreen.push_back(ComboboxItem("Stretch", FULLSCREEN_STRETCH));
    fullscreen.push_back(ComboboxItem("Letterbox (maintain aspect ratio)", FULLSCREEN_ASPECT));
    appFullScreenControl = appSection->addComboboxRow(CONTAINER_VIEW_FULL_SCREEN_LABEL, CONTAINER_VIEW_FULL_SCREEN_HELP, fullscreen);
    appFullScreenControl->onChange = [this]() {
        this->currentAppChanged = true;
    };

    std::vector<ComboboxItem> vsync;
    vsync.push_back(ComboboxItem("Not Set", VSYNC_NOT_SET));
    vsync.push_back(ComboboxItem("Disabled", VSYNC_DISABLED));
    vsync.push_back(ComboboxItem("Enabled", VSYNC_ENABLED));
    vsync.push_back(ComboboxItem("Adaptive", VSYNC_ADAPTIVE));
    appVSyncControl = appSection->addComboboxRow(CONTAINER_VIEW_VSYNC_LABEL, CONTAINER_VIEW_VSYNC_HELP, vsync);
    appVSyncControl->onChange = [this]() {
        this->currentAppChanged = true;
    };

    if (GlobalSettings::isDpiAware()) {
        appDpiAwareControl = appSection->addCheckbox(CONTAINER_VIEW_DPI_AWARE_LABEL, CONTAINER_VIEW_DPI_AWARE_HELP, false);
        appDpiAwareControl->onChange = [this]() {
            this->currentAppChanged = true;
        };
    }
    appPollRateControl = appSection->addTextInputRow(CONTAINER_VIEW_POLL_RATE_LABEL, CONTAINER_VIEW_POLL_RATE_HELP);
    appPollRateControl->onChange = [this]() {
        this->currentApp->pollRate = atoi(appPollRateControl->getText().c_str());
        this->currentAppChanged = true;
    };
    appSkipFramesControl = appSection->addTextInputRow(CONTAINER_VIEW_SKIP_FRAMES_LABEL, CONTAINER_VIEW_SKIP_FRAMES_HELP);
    appSkipFramesControl->onChange = [this]() {
        this->currentApp->skipFramesFPS = atoi(appSkipFramesControl->getText().c_str());
        this->currentAppChanged = true;
    };
#ifdef BOXEDWINE_MULTI_THREADED
    std::vector<ComboboxItem> affinity;
    affinity.push_back(ComboboxItem(getTranslation(GENERIC_COMBOBOX_ALL), 0));
#ifdef __MACH__
    // Platform::setCpuAffinityForThread
    affinity.push_back(ComboboxItem("1", 1));
#else
    for (U32 i = 1; i < Platform::getCpuCount(); i++) {
        affinity.push_back(ComboboxItem(std::to_string(i), i));
    }
#endif
    appCpuAffinityControl = appSection->addComboboxRow(CONTAINER_VIEW_CPU_AFFINITY_LABEL, CONTAINER_VIEW_CPU_AFFINITY_HELP, affinity);
    appCpuAffinityControl->onChange = [this]() {
        this->currentAppChanged = true;
    };
#endif
    std::vector<ComboboxItem> scales;
    scales.push_back(ComboboxItem(getTranslation(GENERIC_DEFAULT), 0));
    scales.push_back(ComboboxItem("1/2x", 50));
    scales.push_back(ComboboxItem("1x", 100));
    scales.push_back(ComboboxItem("2x", 200));
    scales.push_back(ComboboxItem("3x", 300));
    appScaleControl = appSection->addComboboxRow(CONTAINER_VIEW_SCALE_LABEL, CONTAINER_VIEW_SCALE_HELP, scales);
    appScaleControl->onChange = [this]() {
        this->currentAppChanged = true;
    };

    std::vector<ComboboxItem> quality;
    quality.push_back(ComboboxItem("Nearest Pixel Sampling (default)"));
    quality.push_back(ComboboxItem("Linear Filtering"));
    appScaleQualityControl = appSection->addComboboxRow(CONTAINER_VIEW_SCALE_QUALITY_LABEL, CONTAINER_VIEW_SCALE_QUALITY_HELP, quality);
    appScaleQualityControl->onChange = [this]() {
        this->currentAppChanged = true;
    };

#if defined(BOXEDWINE_OPENGL_OSMESA) && defined(BOXEDWINE_OPENGL_SDL)
    std::vector<ComboboxItem> glOptions;
    glOptions.push_back(ComboboxItem(getTranslation(GENERIC_DEFAULT), OPENGL_TYPE_NOT_SET));
    glOptions.push_back(ComboboxItem("Native", OPENGL_TYPE_SDL));
    glOptions.push_back(ComboboxItem("Mesa - OpenGL in Software", OPENGL_TYPE_OSMESA));
    appOpenGlControl = appSection->addComboboxRow(OPTIONSVIEW_DEFAULT_OPENGL_LABEL, OPTIONSVIEW_DEFAULT_OPENGL_HELP, glOptions);
    appOpenGlControl->setWidth((int)GlobalSettings::scaleFloatUIAndFont(250));
    appOpenGlControl->onChange = [this]() {
        this->currentAppChanged = true;
    };
#endif

    row = appSection->addRow(CONTAINER_VIEW_GL_EXT_LABEL, CONTAINER_VIEW_GL_EXT_HELP);
    appGlExControl = row->addTextInput();
    appGlExControl->setNumberOfLines(3);
    appGlExControl->onChange = [this]() {
        this->currentAppChanged = true;
    };
    
    std::shared_ptr<LayoutButtonControl> setButtonControl = row->addButton(getTranslation(CONTAINER_VIEW_GL_EXT_SET_BUTTON_LABEL));
    setButtonControl->onChange = [this]() {
        appGlExControl->setText("GL_EXT_multi_draw_arrays GL_ARB_vertex_program\nGL_ARB_fragment_program GL_ARB_multitexture\nGL_EXT_secondary_color GL_EXT_texture_lod_bias\nGL_NV_texture_env_combine4 GL_ATI_texture_env_combine3\nGL_EXT_texture_filter_anisotropic GL_ARB_texture_env_combine\nGL_EXT_texture_env_combine GL_EXT_texture_compression_s3tc\nGL_ARB_texture_compression GL_EXT_paletted_texture");
        this->currentAppChanged = true;
    };    

    appShowWindowImmediatelyControl = appSection->addCheckbox(CONTAINER_VIEW_SHOW_WINDOW_LABEL, CONTAINER_VIEW_SHOW_WINDOW_HELP, false);
    appShowWindowImmediatelyControl->onChange = [this]() {
        this->currentAppChanged = true;
    };

    for (auto& item : BoxedwineData::getContainers()) {
        addTab(item->getDir(), item->getName(), model, [this, item](bool buttonPressed, BaseViewTab& tab) {
            if (buttonPressed) {
                this->setCurrentContainer(item);
                if (gotoApp.length()) {                    
                    for (auto& app : item->getApps()) {
                        if (app->getIniFilePath() == gotoApp) {
                            setCurrentApp(app);
                            break;
                        }
                    }
                    gotoApp = "";
                }
            }
            }, [item]() {
                if (item->getApps().size()) {
                    const BoxedAppIcon* icon = item->getApps()[0]->getIconTexture((int)ImGui::GetTextLineHeight());
                    if (icon) {
                        ImGui::Image(icon->texture->getTexture(), ImVec2(ImGui::GetTextLineHeight(), ImGui::GetTextLineHeight()));
                        ImGui::SameLine();
                        return;
                    }
                }
                ImGui::Dummy(ImVec2(ImGui::GetTextLineHeight(), 0.0f));
                ImGui::SameLine();                
            }, [this, item]() {
                runOnMainUI([]() {
                    ImGui::OpenPopup("ContainerOptionsPopup");
                    return false;
                    });

                runOnMainUI([this, item]() {
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(GlobalSettings::scaleFloatUI(8.0f), GlobalSettings::scaleFloatUI(8.0f)));
                    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImGui::GetColorU32(ImGuiCol_ScrollbarGrab) | 0xFF000000);
                    bool result = false;
                    if (ImGui::BeginPopup("ContainerOptionsPopup")) {
                        if (ImGui::Selectable(getTranslation(CONTAINER_VIEW_DELETE_BUTTON_LABEL))) {
                            this->deleteContainer(item);
                        }
                        ImGui::EndPopup();
                        result = true;
                    }
                    ImGui::PopStyleColor();
                    ImGui::PopStyleVar();
                    return result;
                    });
            });
        if (item->getDir() == tab) {
            this->tabIndex = this->getTabCount() - 1;
            gotoApp = app;
        }
    }

    std::shared_ptr<LayoutSection> bottomSection = model->addSection();
    bottomSection->addSeparator();
    label = "";
    if (GlobalSettings::hasIconsFont()) {
        label += TRASH_ICON;
        label += " ";
    }
    label += getTranslation(CONTAINER_VIEW_DELETE_BUTTON_LABEL);
    std::shared_ptr<LayoutButtonControl> deleteContainerButton = bottomSection->addButton(0, CONTAINER_VIEW_DELETE_BUTTON_HELP, label);
    deleteContainerButton->onChange = [this]() {
        this->deleteContainer(this->currentContainer);
    };
}

void ContainersView::deleteContainer(BoxedContainer* container) {
    std::string label;
    if (!container->getApps().size()) {
        label = getTranslationWithFormat(CONTAINER_VIEW_DELETE_CONFIRMATION, true, container->getName());
    } else {
        label = "";

        for (auto& app : container->getApps()) {
            if (label.length() != 0) {
                label += ", ";
            }
            label += app->getName();
        }
        label = getTranslationWithFormat(CONTAINER_VIEW_DELETE_CONFIRMATION_WITH_APPS, true, container->getName(), label);
    }
    runOnMainUI([label, this, container]() {
        new YesNoDlg(GENERIC_DLG_CONFIRM_TITLE, label, [this, container](bool yes) {
            if (yes) {
                runOnMainUI([this, container]() {
                    container->deleteContainerFromFilesystem();
                    std::string containerDir;

                    if (currentContainer != container) {
                        containerDir = currentContainer->getDir();
                    }
                    BoxedwineData::reloadContainers();
                    gotoView(VIEW_CONTAINERS, containerDir);
                    return false;
                    });
            }
            });
        return false;
        });
}

bool ContainersView::saveChanges() {
    if (this->currentContainer && containerNameControl->getText().length() == 0) {
        this->errorMsg = getTranslation(CONTAINER_VIEW_ERROR_BLANK_NAME);
    } else if (this->containerMountDriveControl->getSelection() != 0 && containerMountPathControl->getText().length()==0) {
        this->errorMsg = getTranslation(CONTAINER_VIEW_ERROR_MISSING_MOUNT_LOCATION);
    } else if (this->containerMountDriveControl->getSelection() == 0 && containerMountPathControl->getText().length() != 0) {
        this->errorMsg = getTranslation(CONTAINER_VIEW_ERROR_MISSING_MOUNT_DRIVE);
    } else if (this->currentApp && appNameControl->getText().length()==0) {
        this->errorMsg = getTranslation(CONTAINER_VIEW_NAME_REQUIRED);
    }
    if (!this->errorMsg) {
        if (this->currentContainer && this->currentContainerChanged) {
            if (this->currentContainerMountChanged) {
                this->currentContainer->clearMounts();
                if (this->containerMountDriveControl->getSelection() != 0) {
                    this->currentContainer->addNewMount(MountInfo(this->containerMountDriveControl->getSelectionStringValue(), containerMountPathControl->getText(), true));
                }
            }
            this->currentContainer->setWineVersion(this->containerWineVersionControl->getSelectionStringValue());
            this->currentContainer->setWindowsVersion(BoxedwineData::getWinVersions()[this->containerWindowsVersionControl->getSelection()]);
            this->currentContainer->setGDI(containerGdiControl->isChecked());
            this->currentContainer->setMouseWarpOverride(containerMouseWarpControl->getSelectionStringValue());
            this->currentContainer->saveContainer();
            this->currentContainerChanged = false;            
        }
        if (this->currentApp && this->currentAppChanged) {
            std::string ext = this->appGlExControl->getText();
            stringReplaceAll(ext, "\n", " ");
            stringReplaceAll(ext, "  ", " ");
            stringReplaceAll(ext, "  ", " ");
            this->currentApp->glExt = ext;
            this->currentApp->name = appNameControl->getText();
            this->currentApp->args.clear();
            stringSplit(this->currentApp->args, appArgumentsControl->getText(), '\n');
            
            this->currentApp->resolution = appResolutionControl->getSelectionStringValue();
            this->currentApp->bpp = appBppControl->getSelectionIntValue();
            this->currentApp->scale = appScaleControl->getSelectionIntValue();
            this->currentApp->scaleQuality = this->appScaleQualityControl->getSelection();
            if (this->appOpenGlControl) {
                this->currentApp->openGlType = this->appOpenGlControl->getSelectionIntValue();
            }
            this->currentApp->fullScreen = this->appFullScreenControl->getSelection();
            this->currentApp->vsync = this->appVSyncControl->getSelectionIntValue();
            if (GlobalSettings::isDpiAware()) {
                this->currentApp->dpiAware = this->appDpiAwareControl->isChecked();
            }
            this->currentApp->showWindowImmediately = this->appShowWindowImmediatelyControl->isChecked();
#ifdef BOXEDWINE_MULTI_THREADED
            this->currentApp->cpuAffinity = this->appCpuAffinityControl->getSelectionIntValue();
#endif
            this->currentApp->saveApp();
            this->currentAppChanged = false;
            GlobalSettings::reloadApps();
        }
    }
    return this->errorMsg == NULL;
}

void ContainersView::setCurrentApp(BoxedApp* app) {
    this->currentApp = app;
    appNameControl->setText(app->getName());
    appGlExControl->setText(app->glExt);
    

    if (app->link.length()) {
        appPathControl->setText(app->link);
    } else {
        appPathControl->setText(app->path+"/"+app->cmd);
    }
    std::string args;
    for (auto& arg : app->args) {
        if (args.length()) {
            args += "\n";
        }
        args += arg;
    }
    appArgumentsControl->setText(args);
    appArgumentsControl->setNumberOfLines((int)app->args.size() + 1);
    appResolutionControl->setSelectionByLabel(app->resolution);
    appBppControl->setSelectionIntValue(app->bpp);    

    if (app->fullScreen != FULLSCREEN_NOTSET) {
        appScaleControl->setSelection(0);
    } else {
        appScaleControl->setSelectionIntValue(app->scale);
    }
    appScaleControl->setReadOnly(app->fullScreen != FULLSCREEN_NOTSET);
    appScaleQualityControl->setSelection(app->scaleQuality);
    if (this->appOpenGlControl) {
        appOpenGlControl->setSelectionIntValue(app->openGlType);
    }
    appFullScreenControl->setSelectionIntValue(app->fullScreen);
    appVSyncControl->setSelectionIntValue(app->vsync);
    appDpiAwareControl->setCheck(app->dpiAware);
    appPollRateControl->setText(std::to_string(app->pollRate));
    appSkipFramesControl->setText(std::to_string(app->skipFramesFPS));
    appShowWindowImmediatelyControl->setCheck(app->showWindowImmediately);
#ifdef BOXEDWINE_MULTI_THREADED
    appCpuAffinityControl->setSelectionIntValue(app->cpuAffinity);
#endif
}

void ContainersView::rebuildShortcutsCombobox() {
    std::vector<ComboboxItem> apps;
    for (auto& app : this->currentContainer->getApps()) {
        apps.push_back(ComboboxItem(app->getName(), app->getIniFilePath()));
    }
    appPickerControl->setOptions(apps);
}

void ContainersView::setCurrentContainer(BoxedContainer* container) {
    this->currentContainer = container;
    this->currentContainerChanged = false;
    this->currentContainerMountChanged = false;
    section->setTitle(container->getName());
    containerWineVersionControl->setSelectionByLabel(container->getWineVersion());
    containerWindowsVersionControl->setSelectionByLabel(container->getWindowsVersion());
    containerNameControl->setText(container->getName());
    containerLocationControl->setText(container->getDir());

    container->updateCachedSize();

    if (!container->getMounts().size() || !container->getMounts()[0].wine || container->getMounts()[0].localPath.length() != 1) {
        containerMountDriveControl->setSelection(0);
        containerMountPathControl->setText("");
    } else {
        const MountInfo& mount = container->getMounts()[0];
        std::string lowerCase = mount.localPath;
        stringToLower(lowerCase);
        containerMountDriveControl->setSelectionStringValue(lowerCase);
        containerMountPathControl->setText(mount.nativePath);
    }
    appPickerControl->setSelection(0);
    if (this->currentContainer->getApps().size()) {
        setCurrentApp(this->currentContainer->getApps()[0]);
        rebuildShortcutsCombobox();        
    } else {
        this->currentApp = NULL;
    }    
    showAppSection(this->currentContainer->getApps().size() != 0);
    containerGdiControl->setCheck(container->isGDI()); 
    containerMouseWarpControl->setSelectionStringValue(container->getMouseWarpOverride());
}

void ContainersView::showAppSection(bool show) {
    appSection->setHidden(!show);
    appPickerControl->setRowHidden(!show);
}
