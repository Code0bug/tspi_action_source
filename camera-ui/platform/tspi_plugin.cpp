/*
 * tspi Qt QPA 平台插件
 *
 * 参考 qt5base-5.15.2/src/plugins/platforms/linuxfb 实现
 * QT_QPA_PLATFORM=linuxfb 时走 linuxfb 插件
 * QT_QPA_FB_DRM=1 时走 QLinuxFbDrmScreen（DRM） 否则走 QLinuxFbScreen（fbdev）  
 * QLinuxFbDrmScreen.cpp 中已有 DRM 显示实现，不过只用了 primary plane
 *
 * 思路：
 *  可以在 QLinuxFbDrmScreen 中引入 overlay plane + socket 接收预览数据 dma-buf fd
 *  然后使用 overlay plane 显示预览数据，同时使用 primary plane 显示 UI
 *
 * 注意点：
 *   1. primary plane 要在 overlay plane 之上
 *   2. QLinuxFbDrmScreen 用的旧接口，可以考虑换成 atomic 接口
 */