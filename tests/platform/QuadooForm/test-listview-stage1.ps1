param(
	[ValidateSet('Debug', 'Release', 'All')]
	[string] $Configuration = 'All',
	[switch] $Build
)

$ErrorActionPreference = 'Stop'
$listRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$listResults = Join-Path $listRoot 'tests\results'
$listLogs = Join-Path $listResults 'ListViewStage1Logs'
New-Item -ItemType Directory -Force -Path $listLogs | Out-Null
$listConfigurations = if ($Configuration -eq 'All') { @('Debug','Release') } else { @($Configuration) }

foreach ($listConfiguration in $listConfigurations)
{
	if ($Build)
	{
		& (Join-Path $PSScriptRoot 'build-listview-stage1.cmd') $listConfiguration *> (Join-Path $listLogs "$listConfiguration-build.log")
		if ($LASTEXITCODE -ne 0) { throw 'The ListView contract build failed.' }
	}
	$listOutput = Join-Path $listResults "bin\$listConfiguration"
	$listFixture = Join-Path $listRoot 'tests\desktop\QuadooStudio\ApplicationTests\ListViewContract.quadoo'
	& (Join-Path $listOutput 'QuadooFormHarness.exe') --listview-contract-only *> (Join-Path $listLogs "$listConfiguration-native.log")
	if ($LASTEXITCODE -ne 0) { throw 'The native ListView contract tests failed.' }
	& (Join-Path $listOutput 'QuadooApplicationTests.exe') --listview-contract-only $listFixture *> (Join-Path $listLogs "$listConfiguration-script.log")
	if ($LASTEXITCODE -ne 0) { throw 'The script ListView contract tests failed.' }
}
