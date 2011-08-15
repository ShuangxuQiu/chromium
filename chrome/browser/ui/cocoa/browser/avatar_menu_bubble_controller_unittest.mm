// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/browser/avatar_menu_bubble_controller.h"

#include "base/memory/scoped_nsobject.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/profiles/fake_profile_info_interface.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/hyperlink_button_cell.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/common/notification_service.h"
#include "testing/gtest_mac.h"

class FakeBridge : public AvatarMenuModelObserver {
 public:
  void OnAvatarMenuModelChanged(AvatarMenuModel* model) OVERRIDE {}
};

class AvatarMenuBubbleControllerTest : public CocoaTest {
 public:
  virtual void SetUp() {
    info_.reset(new FakeProfileInfo);

    profile1_.reset(
        new AvatarMenuModel::Item(0, FakeProfileInfo::GetTestImage()));
    profile1_->name = ASCIIToUTF16("Test 1");
    info()->mock_profiles()->push_back(profile1_.get());

    profile2_.reset(
        new AvatarMenuModel::Item(1, FakeProfileInfo::GetTestImage()));
    profile2_->name = ASCIIToUTF16("Test 2");
    info()->mock_profiles()->push_back(profile2_.get());

    bridge_ = new FakeBridge;
    model_ = new AvatarMenuModel(info(), bridge(), NULL);

    NSRect frame = [test_window() frame];
    NSPoint point = NSMakePoint(NSMidX(frame), NSMidY(frame));
    controller_ =
        [[AvatarMenuBubbleController alloc] initWithModel:model()
                                                   bridge:bridge()
                                             parentWindow:test_window()
                                               anchoredAt:point];
  }

  FakeProfileInfo* info() { return info_.get(); }
  AvatarMenuBubbleController* controller() { return controller_; }
  AvatarMenuModel* model() { return model_; }
  FakeBridge* bridge() { return bridge_; }

 private:
  scoped_ptr<AvatarMenuModel::Item> profile1_;
  scoped_ptr<AvatarMenuModel::Item> profile2_;

  scoped_ptr<FakeProfileInfo> info_;

  // Weak; releases self.
  AvatarMenuBubbleController* controller_;

  // Weak; owned by |controller_|.
  AvatarMenuModel* model_;
  FakeBridge* bridge_;
};

TEST_F(AvatarMenuBubbleControllerTest, InitialLayout) {
  [controller() showWindow:nil];

  // Two profiles means two item views and the new button.
  NSView* contents = [[controller() window] contentView];
  EXPECT_EQ(3U, [[contents subviews] count]);

  // Loop over the itmes and match the viewController views to subviews.
  NSMutableArray* subviews =
      [NSMutableArray arrayWithArray:[contents subviews]];
  for (AvatarMenuItemController* viewController in [controller() items]) {
    for (NSView* subview in subviews) {
      if ([viewController view] == subview) {
        [subviews removeObject:subview];
        break;
      }
    }
  }

  // The one remaining subview should be the new user button.
  EXPECT_EQ(1U, [subviews count]);

  NSButton* newUser = [subviews lastObject];
  ASSERT_TRUE([newUser isKindOfClass:[NSButton class]]);

  EXPECT_EQ(@selector(newProfile:), [newUser action]);
  EXPECT_EQ(controller(), [newUser target]);
  EXPECT_TRUE([[newUser cell] isKindOfClass:[HyperlinkButtonCell class]]);

  [controller() close];
}

TEST_F(AvatarMenuBubbleControllerTest, PerformLayout) {
  [controller() showWindow:nil];

  NSView* contents = [[controller() window] contentView];
  EXPECT_EQ(3U, [[contents subviews] count]);

  scoped_nsobject<NSMutableArray> oldItems([[controller() items] copy]);

  // Now create a new profile and notify the delegate.
  AvatarMenuModel::Item profile3(2, FakeProfileInfo::GetTestImage());
  profile3.name = ASCIIToUTF16("Test 3");
  info()->mock_profiles()->push_back(&profile3);

  NotificationService::current()->Notify(
      chrome::NOTIFICATION_PROFILE_CACHED_INFO_CHANGED,
      NotificationService::AllSources(),
      NotificationService::NoDetails());

  // Testing the bridge is not worth the effort...
  [controller() performLayout];

  EXPECT_EQ(4U, [[contents subviews] count]);

  // Make sure that none of the old items exit.
  NSArray* newItems = [controller() items];
  for (AvatarMenuItemController* oldVC in oldItems.get()) {
    EXPECT_FALSE([newItems containsObject:oldVC]);
    EXPECT_FALSE([[contents subviews] containsObject:[oldVC view]]);
  }

  [controller() close];
}

TEST_F(AvatarMenuBubbleControllerTest, HighlightForEventType) {
  scoped_nsobject<AvatarMenuItemController> item(
      [[AvatarMenuItemController alloc] initWithModelIndex:0
                                            menuController:nil]);
  NSTextField* field = [item nameField];
  // Test non-active states first.
  [[item activeView] setHidden:YES];

  NSColor* startColor = [NSColor controlTextColor];
  NSColor* upColor = nil;
  NSColor* downColor = nil;
  NSColor* enterColor = nil;
  NSColor* exitColor = nil;

  EXPECT_NSEQ(startColor, field.textColor);

  // The controller does this in |-performLayout|.
  [item highlightForEventType:NSLeftMouseUp];
  EXPECT_NSNE(startColor, field.textColor);
  startColor = field.textColor;

  [item highlightForEventType:NSLeftMouseDown];
  downColor = field.textColor;

  [item highlightForEventType:NSMouseEntered];
  enterColor = field.textColor;

  [item highlightForEventType:NSMouseExited];
  exitColor = field.textColor;

  [item highlightForEventType:NSLeftMouseUp];
  upColor = field.textColor;

  // Use transitivity to determine that all colors for each state are correct.
  EXPECT_NSEQ(startColor, upColor);
  EXPECT_NSEQ(upColor, exitColor);
  EXPECT_NSEQ(downColor, enterColor);
  EXPECT_NSNE(enterColor, exitColor);

  // Make the item "active" and re-test.
  [[item activeView] setHidden:NO];
  [item highlightForEventType:NSLeftMouseUp];
  EXPECT_NSNE(startColor, field.textColor);
  upColor = field.textColor;

  [item highlightForEventType:NSLeftMouseDown];
  EXPECT_NSNE(downColor, field.textColor);
  downColor = field.textColor;

  [item highlightForEventType:NSMouseEntered];
  EXPECT_NSNE(enterColor, field.textColor);
  enterColor = field.textColor;

  [item highlightForEventType:NSMouseExited];
  EXPECT_NSNE(exitColor, field.textColor);
  exitColor = field.textColor;

  EXPECT_NSEQ(upColor, exitColor);
  EXPECT_NSEQ(downColor, enterColor);
  EXPECT_NSNE(upColor, downColor);
}
