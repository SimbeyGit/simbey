param(
	[ValidateSet('Debug', 'Release', 'All')]
	[string] $Configuration = 'All',
	[switch] $Build,
	[switch] $Full
)

$ErrorActionPreference = 'Stop'
$listRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$listLogs = Join-Path $listRoot 'tests\results\ListViewStage2Logs'
New-Item -ItemType Directory -Path $listLogs -Force | Out-Null
$listConfigurations = @('Debug', 'Release')
if ($Configuration -ne 'All')
{
	$listConfigurations = @($Configuration)
}

function Invoke-ListValidation
{
	param([string] $Name, [string] $Program, [string[]] $ProgramArguments)
	$listLog = Join-Path $listLogs "$listConfiguration-$Name.log"
	& $Program @ProgramArguments 2>&1 | Out-File -LiteralPath $listLog -Encoding utf8
	if ($LASTEXITCODE -ne 0)
	{
		Get-Content -LiteralPath $listLog -Tail 35
		throw "ListView validation failed: $listConfiguration $Name (exit $LASTEXITCODE)"
	}
	Write-Output "$listConfiguration $Name passed; $listLog"
	Get-Content -LiteralPath $listLog -Tail 1
}

Push-Location $listRoot
try
{
	foreach ($listConfiguration in $listConfigurations)
	{
		if ($Build)
		{
			$listBuildArguments = @($listConfiguration)
			if ($Full)
			{
				$listBuildArguments += 'Full'
			}
			Invoke-ListValidation 'build' (Join-Path $PSScriptRoot 'build-listview-stage2.cmd') $listBuildArguments
		}
		$listOutput = Join-Path $listRoot "tests\results\bin\$listConfiguration"
		Invoke-ListValidation 'native' (Join-Path $listOutput 'QuadooFormHarness.exe') @('--listview-only')
		Invoke-ListValidation 'rollback' (Join-Path $listOutput 'QuadooFormNativeTests.exe') @('--listview-only')
		if ($Full)
		{
			& (Join-Path $PSScriptRoot 'test-listview-stage1.ps1') -Configuration $listConfiguration
			Invoke-ListValidation 'full-native' (Join-Path $listOutput 'QuadooFormHarness.exe') @('--trace')
			Invoke-ListValidation 'full-private' (Join-Path $listOutput 'QuadooFormNativeTests.exe') @()
			Invoke-ListValidation 'full-designer' (Join-Path $listOutput 'QuadooFormDesignerTests.exe') @('--trace')
			Invoke-ListValidation 'full-studio' (Join-Path $listOutput 'QuadooStudioIntegrationTests.exe') @('--trace')
			Invoke-ListValidation 'full-script' (Join-Path $listOutput 'QuadooApplicationTests.exe') @((Join-Path $listRoot 'tests\desktop\QuadooStudio\ApplicationTests\Runtime.quadoo'))
		}
	}
}
finally
{
	Pop-Location
}
