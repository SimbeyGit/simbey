param(
	[ValidateSet('Debug', 'Release', 'All')]
	[string] $Configuration = 'All',
	[ValidateSet('Win32', 'x64')]
	[string] $Platform = 'Win32',
	[switch] $Build,
	[switch] $Integration,
	[switch] $Full
)

$ErrorActionPreference = 'Stop'
$lifetimeRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$lifetimeLogs = Join-Path $lifetimeRoot 'tests\results\ApplicationLifetimeLogs'
New-Item -ItemType Directory -Path $lifetimeLogs -Force | Out-Null
$lifetimeConfigurations = @('Debug', 'Release')
if ($Configuration -ne 'All')
{
	$lifetimeConfigurations = @($Configuration)
}
$lifetimeFailures = @()

function Invoke-LifetimeCheck
{
	param([string] $Name, [string] $Program, [string[]] $ProgramArguments)
	$lifetimeLog = Join-Path $lifetimeLogs "$lifetimeConfiguration-$Platform-$Name.log"
	$lifetimeError = Join-Path $lifetimeLogs "$lifetimeConfiguration-$Platform-$Name-stderr.log"
	$lifetimeOptions = @{
		FilePath = $Program
		WorkingDirectory = $lifetimeRoot
		WindowStyle = 'Hidden'
		RedirectStandardOutput = $lifetimeLog
		RedirectStandardError = $lifetimeError
		PassThru = $true
	}
	if ($ProgramArguments.Count)
	{
		$lifetimeOptions.ArgumentList = $ProgramArguments
	}
	$lifetimeProcess = Start-Process @lifetimeOptions
	# The full native resource suite measures repeated stabilization cycles.
	$lifetimeTimeout = if ($Name -eq 'native') { 180000 } else { 45000 }
	$lifetimeWatch = [Diagnostics.Stopwatch]::StartNew()
	while (!$lifetimeProcess.WaitForExit(1000))
	{
		if ($lifetimeWatch.ElapsedMilliseconds -ge $lifetimeTimeout)
		{
			# Terminate only the test process started immediately above.
			$lifetimeProcess.Kill()
			$lifetimeProcess.WaitForExit()
			throw "$lifetimeConfiguration $Platform $Name timed out; $lifetimeLog"
		}
	}
	$lifetimeProcess.Refresh()
	$lifetimeExit = $lifetimeProcess.ExitCode
	Write-Output "$lifetimeConfiguration $Platform $Name ($lifetimeExit)"
	Get-Content -LiteralPath $lifetimeLog -Tail 3
	if ($lifetimeExit -ne 0)
	{
		Get-Content -LiteralPath $lifetimeError -Tail 8
		$script:lifetimeFailures += "$lifetimeConfiguration $Platform $Name ($lifetimeExit)"
	}
}

Push-Location $lifetimeRoot
try
{
	foreach ($lifetimeConfiguration in $lifetimeConfigurations)
	{
		if ($Build)
		{
			& (Join-Path $PSScriptRoot 'build-application-lifetime.cmd') $lifetimeConfiguration $Platform
			if ($LASTEXITCODE -ne 0)
			{
				throw 'Build failed; tests were not run against stale output.'
			}
		}
		$lifetimeOutput = Join-Path $lifetimeRoot "tests\results\bin\$lifetimeConfiguration"
		if ($Platform -eq 'x64')
		{
			$lifetimeOutput = Join-Path $lifetimeOutput 'x64'
		}
		$lifetimeScript = Join-Path $lifetimeRoot 'tests\desktop\QuadooStudio\ApplicationTests\LifetimeRuntime.quadoo'
		Invoke-LifetimeCheck 'lifetime' (Join-Path $lifetimeOutput 'QuadooApplicationTests.exe') @('--lifetime-only', "`"$lifetimeScript`"")
		if ($Full -or $Integration)
		{
			$lifetimeScript = Join-Path $lifetimeRoot 'tests\desktop\QuadooStudio\ApplicationTests\Runtime.quadoo'
			Invoke-LifetimeCheck 'application' (Join-Path $lifetimeOutput 'QuadooApplicationTests.exe') @("`"$lifetimeScript`"")
			if ($Full)
			{
				Invoke-LifetimeCheck 'native' (Join-Path $lifetimeOutput 'QuadooFormHarness.exe') @('--trace')
				Invoke-LifetimeCheck 'private' (Join-Path $lifetimeOutput 'QuadooFormNativeTests.exe') @()
			}
			Invoke-LifetimeCheck 'studio-application' (Join-Path $lifetimeOutput 'QuadooStudioIntegrationTests.exe') @('--application-only')
			Invoke-LifetimeCheck 'studio-creation' (Join-Path $lifetimeOutput 'QuadooStudioIntegrationTests.exe') @('--creation-only')
			Invoke-LifetimeCheck 'studio-source' (Join-Path $lifetimeOutput 'QuadooStudioIntegrationTests.exe') @('--source-only')
		}
	}
	if ($lifetimeFailures.Count)
	{
		throw ($lifetimeFailures -join '; ')
	}
}
finally
{
	Pop-Location
}
