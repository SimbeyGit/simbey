param(
	[ValidateSet('Debug', 'Release', 'All')]
	[string] $Configuration = 'All',
	[switch] $Build,
	[switch] $Gui
)

$ErrorActionPreference = 'Stop'
$imageRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$imageLogs = Join-Path $imageRoot 'tests\results\ImageButtonLogs'
New-Item -ItemType Directory -Path $imageLogs -Force | Out-Null
$imageConfigurations = @('Debug', 'Release')
if ($Configuration -ne 'All')
{
	$imageConfigurations = @($Configuration)
}

function Invoke-ImageButtonCheck
{
	param([string] $Name, [string] $Program, [string[]] $ProgramArguments)
	$imageLog = Join-Path $imageLogs "$imageConfiguration-$Name.log"
	& $Program @ProgramArguments 2>&1 | Out-File -LiteralPath $imageLog -Encoding utf8
	$imageExit = $LASTEXITCODE
	Get-Content -LiteralPath $imageLog -Tail 4
	if ($imageExit -ne 0)
	{
		throw "$imageConfiguration $Name failed ($imageExit); $imageLog"
	}
}

Push-Location $imageRoot
try
{
	foreach ($imageConfiguration in $imageConfigurations)
	{
		if ($Build)
		{
			Invoke-ImageButtonCheck 'build' (Join-Path $PSScriptRoot 'build-imagebutton.cmd') @($imageConfiguration)
		}
		$imageOutput = Join-Path $imageRoot "tests\results\bin\$imageConfiguration"
		# Both paths return before GUI/COM initialization. No other test suites
		# are launched unless the user explicitly opts into GUI validation.
		Invoke-ImageButtonCheck 'headless' (Join-Path $imageOutput 'QuadooFormNativeTests.exe') @('--image-button-headless')
		Invoke-ImageButtonCheck 'compile-only' (Join-Path $imageOutput 'QuadooApplicationTests.exe') @('--compile-only', (Join-Path $imageRoot 'tests\desktop\QuadooStudio\ApplicationTests\Runtime.quadoo'))
		if ($Gui)
		{
			Invoke-ImageButtonCheck 'native' (Join-Path $imageOutput 'QuadooFormHarness.exe') @('--image-button-only')
			Invoke-ImageButtonCheck 'designer' (Join-Path $imageOutput 'QuadooFormDesignerTests.exe') @('--image-button-only')
			Invoke-ImageButtonCheck 'studio' (Join-Path $imageOutput 'QuadooStudioIntegrationTests.exe') @('--image-button-only')
			Invoke-ImageButtonCheck 'application' (Join-Path $imageOutput 'QuadooApplicationTests.exe') @((Join-Path $imageRoot 'tests\desktop\QuadooStudio\ApplicationTests\Runtime.quadoo'))
		}
	}
}
finally
{
	Pop-Location
}
