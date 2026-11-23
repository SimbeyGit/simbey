param(
	[ValidateSet('Debug', 'Release', 'All')]
	[string] $Configuration = 'All'
)

$ErrorActionPreference = 'Stop'
$pickerRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$pickerLogs = Join-Path $pickerRoot 'tests\results\DateTimePickerLogs'
New-Item -ItemType Directory -Path $pickerLogs -Force | Out-Null
$pickerConfigurations = @('Debug', 'Release')
if ($Configuration -ne 'All')
{
	$pickerConfigurations = @($Configuration)
}

foreach ($pickerConfiguration in $pickerConfigurations)
{
	foreach ($pickerProgram in @('QuadooFormHarness', 'QuadooFormDesignerTests', 'QuadooStudioIntegrationTests', 'QuadooApplicationTests'))
	{
		$pickerLog = Join-Path $pickerLogs "$pickerConfiguration-$pickerProgram.log"
		$pickerError = Join-Path $pickerLogs "$pickerConfiguration-$pickerProgram-stderr.log"
		$pickerArguments = @('--datetime-picker-only')
		if ($pickerProgram -ne 'QuadooApplicationTests')
		{
			$pickerArguments += '--trace'
		}
		$pickerProcess = Start-Process -FilePath (Join-Path $pickerRoot "tests\results\bin\$pickerConfiguration\$pickerProgram.exe") `
			-ArgumentList $pickerArguments -WorkingDirectory $pickerRoot -WindowStyle Hidden `
			-RedirectStandardOutput $pickerLog -RedirectStandardError $pickerError -PassThru
		$pickerTimer = [Diagnostics.Stopwatch]::StartNew()
		while (!$pickerProcess.WaitForExit(1000))
		{
			if ($pickerTimer.Elapsed.TotalSeconds -gt 120)
			{
				$pickerProcess.Kill()
				throw "$pickerProgram timed out; see $pickerLog"
			}
		}
		$pickerProcess.WaitForExit()
		Write-Output "$pickerConfiguration $pickerProgram"
		Get-Content -LiteralPath $pickerLog -Tail 3
		if ($pickerProcess.ExitCode -ne 0)
		{
			Get-Content -LiteralPath $pickerError -Tail 8
			throw "$pickerProgram failed with exit code $($pickerProcess.ExitCode); see $pickerLog"
		}
	}
}
