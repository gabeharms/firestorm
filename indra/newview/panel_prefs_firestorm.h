/*${License blank}*/
#ifndef panel_prefs_firestorm
#define panel_prefs_firestorm
#include "llfloaterpreference.h"
#include "lllineeditor.h"
class LLLineEditor;
class PanelPreferenceFirestorm : public LLPanelPreference
{
public:
	PanelPreferenceFirestorm();
	
	/*virtual*/ BOOL postBuild();
	/*virtual*/ void apply();
	/*virtual*/ void cancel();

protected:
	LLLineEditor* m_calcLineEditor;
	LLLineEditor* m_acLineEditor;
	LLLineEditor* m_tp2LineEditor;
	LLLineEditor* m_clearchatLineEditor;
	LLLineEditor* m_musicLineEditor;
};
#endif