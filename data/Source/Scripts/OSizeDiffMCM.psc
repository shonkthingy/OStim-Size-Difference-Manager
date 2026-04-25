Scriptname OSizeDiffMCM extends SKI_ConfigBase

Int Property Mode Auto
Float Property Tolerance Auto
Bool Property ApplyToPlayerScenes Auto
Bool Property ApplyToNpcScenes Auto
Bool Property ApplyInAutoMode Auto
Int Property FallbackBehavior Auto

Event OnConfigInit()
    ModName = "OStim Size Difference Manager"
EndEvent

Event OnConfigClose()
    ; v1: DLL reads settings from ini file.
    ; Future: emit explicit event for hot-reload if needed.
EndEvent
