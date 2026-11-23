param(
	[ValidateSet('Debug', 'Release', 'All')]
	[string] $Configuration = 'All',
	[switch] $Build,
	[switch] $StateOnly
)

$ErrorActionPreference = 'Stop'
$asyncRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$asyncLogs = Join-Path $asyncRoot 'tests\results\AsyncWaitLogs'
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
	Invoke-AsyncCheck 'state' (Join-Path $asyncOutput 'QuadooApplicationTests.exe') @('--asyncwait-state-only')
	if (!$StateOnly)
	{
		$asyncScript = Join-Path $asyncRoot 'tests\desktop\QuadooStudio\ApplicationTests\AsyncWaitRuntime.quadoo'
		Invoke-AsyncCheck 'wait' (Join-Path $asyncOutput 'QuadooApplicationTests.exe') @('--asyncwait-only', "`"$asyncScript`"")
	}
}
