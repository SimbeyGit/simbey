param(
	[ValidateSet('Debug', 'Release', 'All')]
	[string] $Configuration = 'All',
	[switch] $Build,
	[switch] $Full,
	[switch] $IsolatedDesktop
)

$ErrorActionPreference = 'Stop'
$script:listFailures = @()
$listRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$listLogs = Join-Path $listRoot 'tests\results\ListViewStage4Logs'
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
	if ($IsolatedDesktop -and [IO.Path]::GetExtension($Program) -eq '.exe')
	{
		$listDesktop = Join-Path $listRoot 'tests\results\bin\QuadooListViewTestDesktop.exe'
		& $listDesktop $Program @ProgramArguments 2>&1 | Out-File -LiteralPath $listLog -Encoding utf8
	}
	else
	{
		& $Program @ProgramArguments 2>&1 | Out-File -LiteralPath $listLog -Encoding utf8
	}
	if ($LASTEXITCODE -ne 0)
	{
		Get-Content -LiteralPath $listLog -Tail 35
		$script:listFailures += "$listConfiguration $Name (exit $LASTEXITCODE)"
		Write-Output "FAILED: $($script:listFailures[-1])"
		return
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
			$listFailureCount = $script:listFailures.Count
			$listBuildArguments = @($listConfiguration)
			if ($Full)
			{
				$listBuildArguments += 'Full'
			}
			Invoke-ListValidation 'build' (Join-Path $PSScriptRoot 'build-listview-stage3.cmd') $listBuildArguments
			if ($script:listFailures.Count -gt $listFailureCount)
			{
				continue
			}
		}
		$listOutput = Join-Path $listRoot "tests\results\bin\$listConfiguration"
		Invoke-ListValidation 'native' (Join-Path $listOutput 'QuadooFormHarness.exe') @('--listview-only')
		Invoke-ListValidation 'virtual' (Join-Path $listOutput 'QuadooFormHarness.exe') @('--listview-virtual-only')
		Invoke-ListValidation 'rollback' (Join-Path $listOutput 'QuadooFormNativeTests.exe') @('--listview-only')
		if ($Full)
		{
			Invoke-ListValidation 'designer-listview' (Join-Path $listOutput 'QuadooFormDesignerTests.exe') @('--listview-only')
			try
			{
				& (Join-Path $PSScriptRoot 'test-listview-stage1.ps1') -Configuration $listConfiguration
			}
			catch
			{
				$script:listFailures += "$listConfiguration stage-one probes: $_"
			}
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
if ($script:listFailures.Count)
{
	throw ($script:listFailures -join '; ')
}
