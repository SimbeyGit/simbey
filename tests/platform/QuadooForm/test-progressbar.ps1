param(
	[ValidateSet('Debug', 'Release', 'All')]
	[string] $Configuration = 'All',
	[switch] $Full
)

$ErrorActionPreference = 'Stop'
$progressRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$progressLogs = Join-Path $progressRoot 'tests\results\ProgressBarLogs'
New-Item -ItemType Directory -Path $progressLogs -Force | Out-Null
$progressConfigurations = @('Debug', 'Release')
if ($Configuration -ne 'All')
{
	$progressConfigurations = @($Configuration)
}

function Invoke-ProgressCheck
{
	param([string] $Name, [string] $Program, [string[]] $ProgramArguments)
	$progressLog = Join-Path $progressLogs "$progressConfiguration-$Name.log"
	$progressError = Join-Path $progressLogs "$progressConfiguration-$Name-stderr.log"
	$progressOptions = @{
		FilePath = Join-Path $progressOutput $Program
		WorkingDirectory = $progressRoot
		WindowStyle = 'Hidden'
		RedirectStandardOutput = $progressLog
		RedirectStandardError = $progressError
		PassThru = $true
	}
	if ($ProgramArguments.Count)
	{
		$progressOptions.ArgumentList = $ProgramArguments
	}
	$progressProcess = Start-Process @progressOptions
	$progressTimer = [Diagnostics.Stopwatch]::StartNew()
	while (!$progressProcess.WaitForExit(1000))
	{
		if ($progressTimer.Elapsed.TotalSeconds -gt 180)
		{
			$progressProcess.Kill()
			throw "$Name timed out; see $progressLog"
		}
	}
	$progressProcess.WaitForExit()
	Get-Content -LiteralPath $progressLog -Tail 4
	if ($progressProcess.ExitCode -ne 0)
	{
		Get-Content -LiteralPath $progressError -Tail 8
		throw "$Name failed with exit code $($progressProcess.ExitCode); see $progressLog"
	}
}

foreach ($progressConfiguration in $progressConfigurations)
{
	$progressOutput = Join-Path $progressRoot "tests\results\bin\$progressConfiguration"
	Invoke-ProgressCheck 'native' 'QuadooFormHarness.exe' @('--progress-bar-only', '--trace')
	Invoke-ProgressCheck 'designer' 'QuadooFormDesignerTests.exe' @('--progress-bar-only', '--trace')
	Invoke-ProgressCheck 'palette' 'QuadooFormDesignerTests.exe' @('--palette-only', '--trace')
	Invoke-ProgressCheck 'studio' 'QuadooStudioIntegrationTests.exe' @('--progress-bar-only', '--trace')
	$progressScript = Join-Path $progressRoot 'tests\desktop\QuadooStudio\ApplicationTests\Runtime.quadoo'
	Invoke-ProgressCheck 'script' 'QuadooApplicationTests.exe' @("`"$progressScript`"")
	if ($Full)
	{
		Invoke-ProgressCheck 'native-full' 'QuadooFormHarness.exe' @('--trace')
		Invoke-ProgressCheck 'private-full' 'QuadooFormNativeTests.exe' @()
	}
}
