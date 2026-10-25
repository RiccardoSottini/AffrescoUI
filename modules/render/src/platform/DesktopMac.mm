/// @file
/// @brief The desktop helpers on macOS (Cocoa, ARC): the colour scheme from the user's defaults, the Dock icon, and the
/// menu bar at the top of the screen. The bar belongs to the app, so each window keeps its menus and shows them when it
/// gets the focus. Items carry no key equivalents: the window's key handling runs shortcuts, so a shortcut never runs
/// twice.

#include "platform/Desktop.hpp"

#include "affresco/render/Bitmap.hpp"
#include "affresco/render/Window.hpp"

#include <GLFW/glfw3.h>

#import <Cocoa/Cocoa.h>

#include <cstddef>
#include <cstring>
#include <utility>

/// @brief The target of a window's menu items: an item's tag is its index in the actions.
@interface AffrescoMenuTarget : NSObject {
@public
	std::vector<std::function<void()>> actions;     ///< What each item does.
	affresco::platform::desktop::MenuChosen chosen; ///< Runs it.
}
/// @brief Runs a chosen item.
/// @param item The item.
- (void)choose:(NSMenuItem*)item;
@end

@implementation AffrescoMenuTarget
- (void)choose:(NSMenuItem*)item {
	const auto index = static_cast<std::size_t>(item.tag);
	if(index < actions.size() && chosen) chosen(actions[index]); // a copy: running it may replace the menus
}
@end

namespace affresco::platform::desktop {

namespace {

/// @brief NSString from UTF-8.
/// @param text The text.
/// @return It.
NSString* string(const std::string& text) { return [NSString stringWithUTF8String:text.c_str()] ?: @""; }

/// @brief The app's own menu (Hide, Quit...), first in every bar as on every Mac app.
/// @return The menu's item.
NSMenuItem* applicationMenu() {
	NSString* name = NSRunningApplication.currentApplication.localizedName ?: @"";
	NSMenu* menu = [[NSMenu alloc] initWithTitle:name];
	[menu addItemWithTitle:[@"About " stringByAppendingString:name]
	                action:@selector(orderFrontStandardAboutPanel:)
	         keyEquivalent:@""];
	[menu addItem:NSMenuItem.separatorItem];
	[menu addItemWithTitle:[@"Hide " stringByAppendingString:name] action:@selector(hide:) keyEquivalent:@"h"];
	NSMenuItem* others = [menu addItemWithTitle:@"Hide Others"
	                                     action:@selector(hideOtherApplications:)
	                              keyEquivalent:@"h"];
	others.keyEquivalentModifierMask = NSEventModifierFlagOption | NSEventModifierFlagCommand;
	[menu addItemWithTitle:@"Show All" action:@selector(unhideAllApplications:) keyEquivalent:@""];
	[menu addItem:NSMenuItem.separatorItem];
	[menu addItemWithTitle:[@"Quit " stringByAppendingString:name] action:@selector(terminate:) keyEquivalent:@"q"];

	NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:name action:nil keyEquivalent:@""];
	item.submenu = menu;
	return item;
}

/// @brief A menu bar on macOS.
class MacMenuBar final : public MenuBar {
public:
	/// @brief An empty bar.
	/// @param window The GLFW window.
	/// @param chosen Runs chosen items.
	MacMenuBar(GLFWwindow* window, MenuChosen chosen)
	    : window_(window), target_([[AffrescoMenuTarget alloc] init]), original_(NSApp.mainMenu) {
		target_->chosen = std::move(chosen);
	}

	/// @brief Gives the app back GLFW's menu if this bar is shown.
	~MacMenuBar() override {
		target_->chosen = nullptr;
		target_->actions.clear();
		if(menu_ && NSApp.mainMenu == menu_) NSApp.mainMenu = original_;
	}

	MacMenuBar(const MacMenuBar&) = delete;
	MacMenuBar& operator=(const MacMenuBar&) = delete;
	MacMenuBar(MacMenuBar&&) = delete;
	MacMenuBar& operator=(MacMenuBar&&) = delete;

	void show(const std::vector<MenuItem>& menus) override {
		const bool shown = menu_ && NSApp.mainMenu == menu_;
		target_->actions.clear();
		menu_ = nil;
		if(!menus.empty()) {
			menu_ = [[NSMenu alloc] initWithTitle:@""];
			[menu_ addItem:applicationMenu()];
			for(const MenuItem& item : menus) [menu_ addItem:build(item)];
		}
		if(shown || glfwGetWindowAttrib(window_, GLFW_FOCUSED) == GLFW_TRUE) activate();
	}

	void activate() override { NSApp.mainMenu = menu_ ? menu_ : original_; }

private:
	/// @brief Builds an item, and its submenu.
	/// @param item The item.
	/// @return It.
	NSMenuItem* build(const MenuItem& item) {
		if(item.separator) return NSMenuItem.separatorItem;
		NSMenuItem* native = [[NSMenuItem alloc] initWithTitle:string(item.title) action:nil keyEquivalent:@""];
		native.enabled = item.enabled;
		if(!item.submenu.empty()) {
			NSMenu* submenu = [[NSMenu alloc] initWithTitle:native.title];
			submenu.autoenablesItems = NO; // enabled as the items say
			for(const MenuItem& child : item.submenu) [submenu addItem:build(child)];
			native.submenu = submenu;
		} else if(item.run) {
			native.target = target_;
			native.action = @selector(choose:);
			native.tag = static_cast<NSInteger>(target_->actions.size());
			target_->actions.push_back(item.run);
		}
		return native;
	}

	GLFWwindow* window_;         ///< Its window.
	AffrescoMenuTarget* target_; ///< Runs the items.
	NSMenu* original_;           ///< GLFW's menu, shown when this window has none.
	NSMenu* menu_ = nil;         ///< This window's, or nil.
};

} // namespace

ColorScheme colorScheme() {
	@autoreleasepool {
		NSString* style = [NSUserDefaults.standardUserDefaults stringForKey:@"AppleInterfaceStyle"];
		return [style isEqualToString:@"Dark"] ? ColorScheme::Dark : ColorScheme::Light;
	}
}

bool reducedMotion() { return NSWorkspace.sharedWorkspace.accessibilityDisplayShouldReduceMotion; }

void watchColorScheme() {
	[NSDistributedNotificationCenter.defaultCenter addObserverForName:@"AppleInterfaceThemeChangedNotification"
	                                                           object:nil
	                                                            queue:NSOperationQueue.mainQueue
	                                                       usingBlock:^(NSNotification*) {
		                                                     glfwPostEmptyEvent();
	                                                       }];
	[NSWorkspace.sharedWorkspace.notificationCenter
	    addObserverForName:NSWorkspaceAccessibilityDisplayOptionsDidChangeNotification
	                object:nil
	                 queue:NSOperationQueue.mainQueue
	            usingBlock:^(NSNotification*) {
		          glfwPostEmptyEvent();
	            }];
}

void setApplicationIcon(const Bitmap* icon) {
	@autoreleasepool {
		if(!icon) {
			NSApp.applicationIconImage = nil; // the bundle's icon again
			return;
		}
		// Cocoa's default bitmap format is the Bitmap's: premultiplied RGBA, 8 bits a channel, top row first
		NSBitmapImageRep* rep = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:nullptr
		                                                                pixelsWide:icon->width()
		                                                                pixelsHigh:icon->height()
		                                                             bitsPerSample:8
		                                                           samplesPerPixel:4
		                                                                  hasAlpha:YES
		                                                                  isPlanar:NO
		                                                            colorSpaceName:NSDeviceRGBColorSpace
		                                                               bytesPerRow:icon->width() * 4
		                                                              bitsPerPixel:32];
		if(!rep) return;
		std::memcpy(rep.bitmapData, icon->pixels().data(), icon->pixels().size());
		NSBitmapImageRep* srgb = [rep bitmapImageRepByRetaggingWithColorSpace:NSColorSpace.sRGBColorSpace] ?: rep;
		NSImage* image = [[NSImage alloc] initWithSize:NSMakeSize(icon->width(), icon->height())];
		[image addRepresentation:srgb];
		NSApp.applicationIconImage = image;
	}
}

std::unique_ptr<MenuBar> createMenuBar(GLFWwindow* window, MenuChosen chosen) {
	return std::make_unique<MacMenuBar>(window, std::move(chosen));
}

} // namespace affresco::platform::desktop
