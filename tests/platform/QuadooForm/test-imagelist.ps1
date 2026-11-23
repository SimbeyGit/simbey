param(
	[ValidateSet('Debug', 'Release', 'All')]
	[string] $Configuration = 'All',
	[ValidateSet('Win32', 'x64', 'All')]
	[string] $Platform = 'All'
)

$ErrorActionPreference = 'Stop'
$imageListRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$imageListConfigurations = @('Debug', 'Release')
$imageListPlatforms = @('Win32', 'x64')
if ($Configuration -ne 'All')
{
	$imageListConfigurations = @($Configuration)
}
if ($Platform -ne 'All')
{
	$imageListPlatforms = @($Platform)
}

foreach ($imageListConfiguration in $imageListConfigurations)
{
	foreach ($imageListPlatform in $imageListPlatforms)
	{
		$imageListOutput = Join-Path $imageListRoot "tests\results\bin\$imageListConfiguration"
		if ($imageListPlatform -eq 'x64')
		{
			$imageListOutput = Join-Path $imageListOutput 'x64'
		}
		foreach ($imageListExecutable in @('QuadooFormHarness.exe', 'QuadooFormDesignerTests.exe', 'QuadooStudioIntegrationTests.exe'))
		{
			Write-Output "$imageListConfiguration $imageListPlatform $imageListExecutable --image-list-only"
			& (Join-Path $imageListOutput $imageListExecutable) --image-list-only
			if ($LASTEXITCODE -ne 0)
			{
				throw "ImageList validation failed: $imageListConfiguration $imageListPlatform $imageListExecutable"
			}
		}
	}
}
