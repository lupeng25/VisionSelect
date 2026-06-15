@echo off
SetLocal EnableDelayedExpansion
(set PATH=D:\Qt\Qt5.12.9\5.12.9\msvc2015_64\bin;!PATH!)
if defined QT_PLUGIN_PATH (
    set QT_PLUGIN_PATH=D:\Qt\Qt5.12.9\5.12.9\msvc2015_64\plugins;!QT_PLUGIN_PATH!
) else (
    set QT_PLUGIN_PATH=D:\Qt\Qt5.12.9\5.12.9\msvc2015_64\plugins
)
%*
EndLocal
