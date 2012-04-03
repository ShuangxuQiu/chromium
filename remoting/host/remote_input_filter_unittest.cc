// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/remote_input_filter.h"

#include "remoting/proto/event.pb.h"
#include "remoting/protocol/input_event_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::ExpectationSet;
using ::testing::InSequence;

namespace remoting {

MATCHER_P2(EqualsKeyEvent, keycode, pressed, "") {
  return arg.keycode() == keycode && arg.pressed() == pressed;
}

class MockInputStub : public protocol::InputStub {
 public:
  MockInputStub() {}

  MOCK_METHOD1(InjectKeyEvent, void(const protocol::KeyEvent&));
  MOCK_METHOD1(InjectMouseEvent, void(const protocol::MouseEvent&));
 private:
  DISALLOW_COPY_AND_ASSIGN(MockInputStub);
};

static protocol::MouseEvent MouseMoveEvent(int x, int y) {
  protocol::MouseEvent event;
  event.set_x(x);
  event.set_y(y);
  return event;
}

static protocol::KeyEvent KeyEvent(int keycode, bool pressed) {
  protocol::KeyEvent event;
  event.set_keycode(keycode);
  event.set_pressed(pressed);
  return event;
}

// Verify that events get through if there is no local activity.
TEST(RemoteInputFilterTest, NoLocalActivity) {
  MockInputStub mock_stub;
  protocol::InputEventTracker input_tracker(&mock_stub);
  RemoteInputFilter input_filter(&input_tracker);

  EXPECT_CALL(mock_stub, InjectMouseEvent(_))
        .Times(10);

  for (int i = 0; i < 10; ++i)
    input_filter.InjectMouseEvent(MouseMoveEvent(0, 0));
}

// Verify that events get through until there is local activity.
TEST(RemoteInputFilterTest, MismatchedLocalActivity) {
  MockInputStub mock_stub;
  protocol::InputEventTracker input_tracker(&mock_stub);
  RemoteInputFilter input_filter(&input_tracker);

  EXPECT_CALL(mock_stub, InjectMouseEvent(_))
      .Times(5);

  for (int i = 0; i < 10; ++i) {
    input_filter.InjectMouseEvent(MouseMoveEvent(0, 0));
    if (i == 4)
      input_filter.LocalMouseMoved(SkIPoint::Make(1, 1));
  }
}

// Verify that echos of injected events don't block activity.
TEST(RemoteInputFilterTest, LocalEchoesOfRemoteActivity) {
  MockInputStub mock_stub;
  protocol::InputEventTracker input_tracker(&mock_stub);
  RemoteInputFilter input_filter(&input_tracker);

  EXPECT_CALL(mock_stub, InjectMouseEvent(_))
      .Times(10);

  for (int i = 0; i < 10; ++i) {
    input_filter.InjectMouseEvent(MouseMoveEvent(0, 0));
    input_filter.LocalMouseMoved(SkIPoint::Make(0, 0));
  }
}

// Verify that echos followed by a mismatch blocks activity.
TEST(RemoteInputFilterTest, LocalEchosAndLocalActivity) {
  MockInputStub mock_stub;
  protocol::InputEventTracker input_tracker(&mock_stub);
  RemoteInputFilter input_filter(&input_tracker);

  EXPECT_CALL(mock_stub, InjectMouseEvent(_))
      .Times(5);

  for (int i = 0; i < 10; ++i) {
    input_filter.InjectMouseEvent(MouseMoveEvent(0, 0));
    input_filter.LocalMouseMoved(SkIPoint::Make(0, 0));
    if (i == 4)
      input_filter.LocalMouseMoved(SkIPoint::Make(1, 1));
  }
}

// Verify that local activity also causes buttons & keys to be released.
TEST(RemoteInputFilterTest, LocalActivityReleasesAll) {
  MockInputStub mock_stub;
  protocol::InputEventTracker input_tracker(&mock_stub);
  RemoteInputFilter input_filter(&input_tracker);

  EXPECT_CALL(mock_stub, InjectMouseEvent(_))
      .Times(5);

  // Use release of a key as a proxy for InputEventTracker::ReleaseAll()
  // having been called, rather than mocking it.
  EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsKeyEvent(0, true)));
  EXPECT_CALL(mock_stub, InjectKeyEvent(EqualsKeyEvent(0, false)));
  input_filter.InjectKeyEvent(KeyEvent(0, true));

  for (int i = 0; i < 10; ++i) {
    input_filter.InjectMouseEvent(MouseMoveEvent(0, 0));
    input_filter.LocalMouseMoved(SkIPoint::Make(0, 0));
    if (i == 4)
      input_filter.LocalMouseMoved(SkIPoint::Make(1, 1));
  }
}

}  // namespace remoting
