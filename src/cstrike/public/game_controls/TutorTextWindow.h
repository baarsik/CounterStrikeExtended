#pragma once

class CTutorMessageBox;

class CTutorTextWindow
{
public:
    void RefreshLanguage();
	CTutorTextWindow( int hardwareMode );
	virtual ~CTutorTextWindow();
	virtual void Activate( const wchar_t* message, int msgClass, int isSpectator );
	virtual void AnimateOffscreen();
	void GetPosition( int& x, int& y );
    void ReplaceText(const wchar_t* text);
	int ComputeWindowHeight( int screenheight );

private:
	CTutorMessageBox*	m_messageBox;
	int					m_spectatorPosition;
	int					m_hardwareMode;
};
