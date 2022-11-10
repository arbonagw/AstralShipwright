// Astral Shipwright - Gwennaël Arbona

#include "NovaGameUserSettings.h"
#include "Nova.h"

/*----------------------------------------------------
    Constructor
----------------------------------------------------*/

UNovaGameUserSettings::UNovaGameUserSettings()
{}

/*----------------------------------------------------
    Inherited
----------------------------------------------------*/

void UNovaGameUserSettings::SetToDefaults()
{
	Super::SetToDefaults();

	EnableCameraDegradation = true;
	FOV                     = 100.0f;
}
