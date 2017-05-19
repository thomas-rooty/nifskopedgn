#ifndef SETTINGS_H
#define SETTINGS_H

#include <QSettings>

//! Wrapper for QSettings to manage settings portability
class NSSettings : public QSettings
{
public:
	NSSettings() : QSettings( "nifskope.ini", QSettings::IniFormat )
	{
	}

private:
	NSSettings( const NSSettings & ) = delete;
	NSSettings & operator=( const NSSettings & ) = delete;
};


#endif // SETTINGS_H
