static HRESULT ColorStorageTests()
{
	HRESULT hr;
	const DWORD stored = QuadooForm::PropertySerializable;
	const QF::PropertySpec colors[] = {
		{L"Accent",QuadooProperty::Color,stored,0xffffff,50},
		{L"ForeColor",QuadooProperty::Color,stored,RGB(210,220,230),20}};
	const QF::PropertySpec invalid[] = {{L"Accent",QuadooProperty::Color,stored,0x01000000,50}};
	QF::TypeSpec type = {L"ColorProbe",&QF::FormType,colors,ARRAYSIZE(colors),NULL,0};
	TStackRef<CProbeFactory> factory;
	TStackRef<CProbe> probe;
	TStackRef<IQuadooConstructionContext> context;
	TStackRef<IJSONObject> definition;
	TStackRef<IJSONValue> value;
	QuadooForm::ContextOptions options = {sizeof(options)};
	options.DPI = 96;
	factory.Attach(__new CProbeFactory(&type));
	CheckAlloc(factory);
	Check(factory->Initialize());
	Check(QuadooCreateConstructionContext(&options,&context));
	probe.Attach(__new CProbe(factory,context,NULL));
	CheckAlloc(probe);
	Check(JSONCreateObject(&definition));
	Check(probe->Load(definition));
	Check(probe->GetValueJSON(probe->Index(L"ForeColor"),&value));
	INT number;
	Check(value->GetInteger(&number));
	VERIFY(number == RGB(210,220,230));
	value.Release();
	Check(probe->GetValueJSON(probe->Index(L"Accent"),&value));
	Check(value->GetInteger(&number));
	VERIFY(number == 0xffffff);
	Check(probe->SetValueInt(probe->Index(L"Accent"),static_cast<INT>(0x80000000 | COLOR_HIGHLIGHT)));
	value.Release();
	Check(probe->GetValueJSON(probe->Index(L"Accent"),&value));
	Check(value->GetInteger(&number));
	VERIFY(number == static_cast<INT>(0x80000000 | COLOR_HIGHLIGHT));
	VERIFY(probe->SetValueInt(probe->Index(L"Accent"),0x01000000) == E_INVALIDARG);
	value.Release();
	Check(probe->GetValueJSON(probe->Index(L"Accent"),&value));
	Check(value->GetInteger(&number));
	VERIFY(number == static_cast<INT>(0x80000000 | COLOR_HIGHLIGHT));
	Check(probe->SetValueInt(probe->Index(L"Accent"),0xffffff));
	value.Release();
	Check(JSONCreateLongInteger(INT_MIN,&value));
	Check(probe->SetValueJSON(probe->Index(L"Accent"),value));
	Check(probe->GetValueInt(probe->Index(L"Accent"),&number));
	VERIFY(number == INT_MIN);
	value.Release();
	Check(JSONCreateLongInteger(static_cast<LONGLONG>(INT_MIN) - 1,&value));
	VERIFY(probe->SetValueJSON(probe->Index(L"Accent"),value) == E_INVALIDARG);
	Check(probe->GetValueInt(probe->Index(L"Accent"),&number));
	VERIFY(number == INT_MIN);
	type.properties = invalid; type.propertyCount = ARRAYSIZE(invalid);
	Check(MetadataFailure(&type));
Cleanup:
	if(probe) probe->Destroy();
	return hr;
}
