#pragma once
static const WCHAR MenuTestDefinition[] =
L"{\"type\":\"Form\",\"name\":\"MenuForm\",\"id\":\"{F2000000-0000-0000-0000-000000000001}\","
L"\"properties\":{\"Width\":380,\"Height\":180,\"MainMenu\":\"{F2000000-0000-0000-0000-000000000010}\",\"PopupMenu\":\"{F2000000-0000-0000-0000-000000000020}\"},"
L"\"children\":[{\"type\":\"Button\",\"name\":\"ChildButton\",\"properties\":{\"Left\":11,\"Top\":13,\"Width\":80,\"Height\":25}}],\"components\":["
L"{\"type\":\"MainMenu\",\"name\":\"MainMenu1\",\"id\":\"{F2000000-0000-0000-0000-000000000010}\"},"
L"{\"type\":\"MenuItem\",\"name\":\"FileMenu\",\"id\":\"{F2000000-0000-0000-0000-000000000011}\",\"menuParent\":\"{F2000000-0000-0000-0000-000000000010}\",\"properties\":{\"Caption\":\"&File\"}},"
L"{\"type\":\"MenuItem\",\"name\":\"OpenItem\",\"id\":\"{F2000000-0000-0000-0000-000000000012}\",\"menuParent\":\"{F2000000-0000-0000-0000-000000000011}\",\"properties\":{\"Caption\":\"&Open\",\"ShortCut\":\"Ctrl+O\",\"AutoCheck\":true},\"events\":{\"OnClick\":\"MenuClick\"}},"
L"{\"type\":\"MenuItem\",\"name\":\"RecentMenu\",\"id\":\"{F2000000-0000-0000-0000-000000000013}\",\"menuParent\":\"{F2000000-0000-0000-0000-000000000011}\",\"properties\":{\"Caption\":\"&Recent\",\"Order\":2}},"
L"{\"type\":\"MenuItem\",\"name\":\"RecentItem\",\"id\":\"{F2000000-0000-0000-0000-000000000014}\",\"menuParent\":\"{F2000000-0000-0000-0000-000000000013}\",\"properties\":{\"Caption\":\"Nested item\",\"ShortCut\":\"Ctrl+Shift+N\"},\"events\":{\"OnClick\":\"MenuClick\"}},"
L"{\"type\":\"MenuItem\",\"name\":\"Separator1\",\"id\":\"{F2000000-0000-0000-0000-000000000015}\",\"menuParent\":\"{F2000000-0000-0000-0000-000000000011}\",\"properties\":{\"Caption\":\"-\",\"Separator\":true,\"Order\":1}},"
L"{\"type\":\"MenuItem\",\"name\":\"DisabledItem\",\"id\":\"{F2000000-0000-0000-0000-000000000016}\",\"menuParent\":\"{F2000000-0000-0000-0000-000000000011}\",\"properties\":{\"Caption\":\"Disabled\",\"Enabled\":false,\"Checked\":true,\"Order\":3}},"
L"{\"type\":\"MenuItem\",\"name\":\"EditMenu\",\"id\":\"{F2000000-0000-0000-0000-000000000017}\",\"menuParent\":\"{F2000000-0000-0000-0000-000000000010}\",\"properties\":{\"Caption\":\"&Editing commands\",\"Order\":1}},"
L"{\"type\":\"MenuItem\",\"name\":\"LongMenu\",\"id\":\"{F2000000-0000-0000-0000-000000000018}\",\"menuParent\":\"{F2000000-0000-0000-0000-000000000010}\",\"properties\":{\"Caption\":\"&Long menu caption for wrapping\",\"Order\":2}},"
L"{\"type\":\"PopupMenu\",\"name\":\"PopupMenu1\",\"id\":\"{F2000000-0000-0000-0000-000000000020}\"},"
L"{\"type\":\"MenuItem\",\"name\":\"PopupItem\",\"id\":\"{F2000000-0000-0000-0000-000000000021}\",\"menuParent\":\"{F2000000-0000-0000-0000-000000000020}\",\"properties\":{\"Caption\":\"Popup action\"},\"events\":{\"OnClick\":\"MenuClick\"}}]}";
