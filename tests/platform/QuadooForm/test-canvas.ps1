param(
	[ValidateSet('Debug', 'Release', 'All')]
	[string] $Configuration = 'All',
	[switch] $Headless,
	[switch] $Regression
)

$ErrorActionPreference = 'Stop'
$canvasRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$canvasLogs = Join-Path $canvasRoot 'tests\results\CanvasLogs'
New-Item -ItemType Directory -Path $canvasLogs -Force | Out-Null
$canvasConfigurations = @('Debug', 'Release')
if ($Configuration -ne 'All')
{
	$canvasConfigurations = @($Configuration)
}
if ($Headless -and $Regression)
{
	throw 'The full regression suites create HWNDs; use -Headless without -Regression.'
}

function Invoke-CanvasCheck
{
	param([string] $Name, [string] $Program, [string[]] $ProgramArguments)
	$canvasLog = Join-Path $canvasLogs "$canvasConfiguration-$Name.log"
	$canvasError = $canvasLog + '.err'
	$canvasOptions = @{
		FilePath = Join-Path $canvasOutput $Program
		WorkingDirectory = $canvasRoot
		WindowStyle = 'Hidden'
		RedirectStandardOutput = $canvasLog
		RedirectStandardError = $canvasError
		PassThru = $true
	}
	if ($ProgramArguments.Count)
	{
		$canvasOptions.ArgumentList = $ProgramArguments
	}
	$canvasProcess = Start-Process @canvasOptions
	$canvasTimer = [Diagnostics.Stopwatch]::StartNew()
	while (!$canvasProcess.WaitForExit(1000))
	{
		if ($canvasTimer.Elapsed.TotalSeconds -gt 180)
		{
			$canvasProcess.Kill()
			throw "$Name timed out; see $canvasLog"
		}
	}
	$canvasProcess.WaitForExit()
	Write-Output "$canvasConfiguration $Name"
	Get-Content -LiteralPath $canvasLog -Tail 4
	if ($canvasProcess.ExitCode -ne 0)
	{
		Get-Content -LiteralPath $canvasError -Tail 8
		throw "$Name failed with exit code $($canvasProcess.ExitCode); see $canvasLog"
	}
}

foreach ($canvasConfiguration in $canvasConfigurations)
{
	$canvasOutput = Join-Path $canvasRoot "tests\results\bin\$canvasConfiguration"
	# This selector returns before COM/HWND initialization and has no message loop.
	# It still uses offscreen Windows GDI for fonts and DIB sections.
	Invoke-CanvasCheck 'offscreen' 'QuadooFormNativeTests.exe' @('--canvas-rendering-only')
	Invoke-CanvasCheck 'sprites-offscreen' 'QuadooApplicationTests.exe' @('--sprite-headless')
	if (!$Headless)
	{
		Invoke-CanvasCheck 'native' 'QuadooFormHarness.exe' @('--canvas-only', '--trace')
		Invoke-CanvasCheck 'designer' 'QuadooFormDesignerTests.exe' @('--canvas-only', '--trace')
		Invoke-CanvasCheck 'studio' 'QuadooStudioIntegrationTests.exe' @('--canvas-only', '--trace')
		$canvasScript = Join-Path $canvasRoot 'tests\desktop\QuadooStudio\ApplicationTests\Runtime.quadoo'
		Invoke-CanvasCheck 'script' 'QuadooApplicationTests.exe' @('--canvas-only', "`"$canvasScript`"")
		Invoke-CanvasCheck 'palette' 'QuadooFormDesignerTests.exe' @('--palette-only', '--trace')
	}
	if ($Regression)
	{
		Invoke-CanvasCheck 'native-regression' 'QuadooFormHarness.exe' @('--trace')
		Invoke-CanvasCheck 'private-regression' 'QuadooFormNativeTests.exe' @()
	}
}
