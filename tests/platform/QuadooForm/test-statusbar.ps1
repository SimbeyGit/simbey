param(
	[ValidateSet('Debug', 'Release', 'All')]
	[string] $Configuration = 'All',
	[switch] $Full
)

$ErrorActionPreference = 'Stop'
$statusRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$statusLogs = Join-Path $statusRoot 'tests\results\StatusBarLogs'
New-Item -ItemType Directory -Path $statusLogs -Force | Out-Null
$statusConfigurations = @('Debug', 'Release')
if ($Configuration -ne 'All')
{
	$statusConfigurations = @($Configuration)
}

function Invoke-StatusCheck
{
	param([string] $Name, [string] $Program, [string[]] $ProgramArguments)
	$statusLog = Join-Path $statusLogs "$statusConfiguration-$Name.log"
	$statusError = Join-Path $statusLogs "$statusConfiguration-$Name-stderr.log"
	$statusOptions = @{
		FilePath = Join-Path $statusOutput $Program
		WorkingDirectory = $statusRoot
		WindowStyle = 'Hidden'
		RedirectStandardOutput = $statusLog
		RedirectStandardError = $statusError
		PassThru = $true
	}
	if ($ProgramArguments.Count)
	{
		$statusOptions.ArgumentList = $ProgramArguments
	}
	$statusProcess = Start-Process @statusOptions
	$statusTimer = [Diagnostics.Stopwatch]::StartNew()
	while (!$statusProcess.WaitForExit(1000))
	{
		if ($statusTimer.Elapsed.TotalSeconds -gt 180)
		{
			$statusProcess.Kill()
			throw "$Name timed out; see $statusLog"
		}
	}
	$statusProcess.WaitForExit()
	Get-Content -LiteralPath $statusLog -Tail 4
	if ($statusProcess.ExitCode -ne 0)
	{
		Get-Content -LiteralPath $statusError -Tail 8
		throw "$Name failed with exit code $($statusProcess.ExitCode); see $statusLog"
	}
}

foreach ($statusConfiguration in $statusConfigurations)
{
	$statusOutput = Join-Path $statusRoot "tests\results\bin\$statusConfiguration"
	Invoke-StatusCheck 'native' 'QuadooFormHarness.exe' @('--status-bar-only', '--trace')
	Invoke-StatusCheck 'designer' 'QuadooFormDesignerTests.exe' @('--status-bar-only', '--trace')
	Invoke-StatusCheck 'palette' 'QuadooFormDesignerTests.exe' @('--palette-only', '--trace')
	Invoke-StatusCheck 'studio' 'QuadooStudioIntegrationTests.exe' @('--status-bar-only', '--trace')
	$statusScript = Join-Path $statusRoot 'tests\desktop\QuadooStudio\ApplicationTests\Runtime.quadoo'
	Invoke-StatusCheck 'script' 'QuadooApplicationTests.exe' @("`"$statusScript`"")
	if ($Full)
	{
		Invoke-StatusCheck 'native-full' 'QuadooFormHarness.exe' @('--trace')
		Invoke-StatusCheck 'private-full' 'QuadooFormNativeTests.exe' @()
	}
}
