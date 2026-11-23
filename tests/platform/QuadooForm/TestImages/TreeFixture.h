#pragma once
static const WCHAR TreeChildDefinition[] =
	L"{\"type\":\"TreeView\",\"name\":\"Tree1\",\"properties\":{\"Width\":240,\"Height\":180,\"CheckBoxes\":true,\"ReadOnly\":false,"
	L"\"Images\":\"{A9000000-0000-0000-0000-000000000001}\",\"Items\":[{\"Text\":\"Root\",\"ImageIndex\":0,\"Items\":[{\"Text\":\"Child\",\"ImageIndex\":1}]},{\"Text\":\"Second\"}]},"
	L"\"events\":{\"OnChanging\":\"Event\",\"OnChange\":\"Event\",\"OnExpanding\":\"Event\",\"OnExpanded\":\"Event\",\"OnCollapsing\":\"Event\",\"OnCollapsed\":\"Event\","
	L"\"OnEditing\":\"Event\",\"OnEdited\":\"Event\",\"OnCheck\":\"Event\"}}";
