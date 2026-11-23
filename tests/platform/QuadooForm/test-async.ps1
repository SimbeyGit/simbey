param(
	[ValidateSet('Debug', 'Release', 'All')]
	[string] $Configuration = 'All',
	[switch] $Build,
	[switch] $Full
)

$ErrorActionPreference = 'Stop'
$asyncRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$asyncLogs = Join-Path $asyncRoot 'tests\results\AsyncLogs'
New-Item -ItemType Directory -Path $asyncLogs -Force | Out-Null
$asyncConfigurations = @('Debug', 'Release')
if ($Configuration -ne 'All')
{
	$asyncConfigurations = @($Configuration)
}

function Invoke-AsyncCheck
{
	param([string] $Name, [string] $Program, [string[]] $ProgramArguments)
	$asyncLog = Join-Path $asyncLogs "$asyncConfiguration-$Name.log"
	$asyncError = Join-Path $asyncLogs "$asyncConfiguration-$Name-stderr.log"
	$asyncOptions = @{
		FilePath = $Program
		WorkingDirectory = $asyncRoot
		WindowStyle = 'Hidden'
		RedirectStandardOutput = $asyncLog
		RedirectStandardError = $asyncError
		PassThru = $true
	}
	if ($ProgramArguments.Count)
	{
		$asyncOptions.ArgumentList = $ProgramArguments
	}
	$asyncProcess = Start-Process @asyncOptions
	$asyncTimer = [Diagnostics.Stopwatch]::StartNew()
	while (!$asyncProcess.WaitForExit(1000))
	{
		if ($asyncTimer.Elapsed.TotalSeconds -gt 240)
		{
			$asyncProcess.Kill()
			throw "$Name timed out; see $asyncLog"
		}
	}
	$asyncProcess.WaitForExit()
	Get-Content -LiteralPath $asyncLog -Tail 3
	if ($asyncProcess.ExitCode -ne 0)
	{
		Get-Content -LiteralPath $asyncError -Tail 8
		throw "$Name failed with exit code $($asyncProcess.ExitCode); see $asyncLog"
	}
}

foreach ($asyncConfiguration in $asyncConfigurations)
{
	if ($Build)
	{
		$asyncBuild = Join-Path $PSScriptRoot 'build-application-lifetime.cmd'
		$asyncBuildCommand = '"{0}" {1} Win32' -f $asyncBuild, $asyncConfiguration
		Invoke-AsyncCheck 'build' $env:ComSpec @('/d', '/s', '/c', "`"$asyncBuildCommand`"")
	}
	$asyncOutput = Join-Path $asyncRoot "tests\results\bin\$asyncConfiguration"
	$asyncScript = Join-Path $asyncRoot 'tests\desktop\QuadooStudio\ApplicationTests\AsyncRuntime.quadoo'
	Invoke-AsyncCheck 'async' (Join-Path $asyncOutput 'QuadooApplicationTests.exe') @('--async-only', "`"$asyncScript`"")
	$asyncScript = Join-Path $asyncRoot 'tests\desktop\QuadooStudio\ApplicationTests\LifetimeRuntime.quadoo'
	Invoke-AsyncCheck 'lifetime' (Join-Path $asyncOutput 'QuadooApplicationTests.exe') @('--lifetime-only', "`"$asyncScript`"")
	$asyncScript = Join-Path $asyncRoot 'tests\desktop\QuadooStudio\ApplicationTests\Runtime.quadoo'
	Invoke-AsyncCheck 'application' (Join-Path $asyncOutput 'QuadooApplicationTests.exe') @("`"$asyncScript`"")
	if ($Full)
	{
		Invoke-AsyncCheck 'native-full' (Join-Path $asyncOutput 'QuadooFormHarness.exe') @('--trace')
		Invoke-AsyncCheck 'private-full' (Join-Path $asyncOutput 'QuadooFormNativeTests.exe') @()
		Invoke-AsyncCheck 'palette' (Join-Path $asyncOutput 'QuadooFormDesignerTests.exe') @('--palette-only', '--trace')
		Invoke-AsyncCheck 'studio' (Join-Path $asyncOutput 'QuadooStudioIntegrationTests.exe') @('--application-only', '--trace')
	}
}
