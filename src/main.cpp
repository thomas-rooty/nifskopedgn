/*
** main
**
** Copyright © NIFTools, 2017.
** This file is part of NIFSKOPE (https://github.com/niftools/nifskope)
**
** NifSkope is free software; this file may be used under the terms of the GNU
** General Public License version 3.0 or later as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.
**
** NifSkope is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
**
** Please review the following information to ensure the GNU General Public
** License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** You should have received a copy of the GNU General Public License version
** 3.0 along with NifSkope; if not, write to the Free Software Foundation,
** Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
**
** Created Date: 19-May-2017
*/

#include <QApplication>
#include <QDir>
#include <QSettings>
#include <QLocale>
#include <QCommandLineParser>

#include "nifskope.h"
#include "kfmmodel.h"
#include "nifmodel.h"
#include "version.h"

QCoreApplication * createApplication( int &argc, char *argv[] )
{
	// Iterate over args
	for ( int i = 1; i < argc; ++i ) {
		// -no-gui: start as core app without all the GUI overhead
		if ( !qstrcmp( argv[i], "-no-gui" ) ) {
			return new QCoreApplication( argc, argv );
		}
	}
	return new QApplication( argc, argv );
}


/*
 *  main
 */

//! The main program
int main( int argc, char * argv[] )
{
	QScopedPointer<QCoreApplication> app( createApplication( argc, argv ) );

	if ( auto a = qobject_cast<QApplication *>(app.data()) ) {

		a->setOrganizationName( "NifTools" );
		a->setOrganizationDomain( "niftools.org" );
		a->setApplicationName( "NifSkope " + NifSkopeVersion::rawToMajMin( NIFSKOPE_VERSION ) );
		a->setApplicationVersion( NIFSKOPE_VERSION );
		a->setApplicationDisplayName( "NifSkope " + NifSkopeVersion::rawToDisplay( NIFSKOPE_VERSION, true ) );

		// Must set current directory or this causes issues with several features
		QDir::setCurrent( qApp->applicationDirPath() );

		// Register message handler
		//qRegisterMetaType<Message>( "Message" );
		qInstallMessageHandler( NifSkope::MyMessageOutput );

		// Register types
		qRegisterMetaType<NifValue>( "NifValue" );
		QMetaType::registerComparators<NifValue>();

		// Find stylesheet
		QDir qssDir( QApplication::applicationDirPath() );
		QStringList qssList( QStringList()
			<< "style.qss"
#ifdef Q_OS_LINUX
			<< "/usr/share/nifskope/style.qss"
#endif
		);
		QString qssName;
		for ( const QString& str : qssList ) {
			if ( qssDir.exists( str ) ) {
				qssName = qssDir.filePath( str );
				break;
			}
		}

		// Load stylesheet
		if ( !qssName.isEmpty() ) {
			QFile style( qssName );

			if ( style.open( QFile::ReadOnly ) ) {
				a->setStyleSheet( style.readAll() );
				style.close();
			}
		}

		// Set locale
		QSettings cfg( QString( "%1/nifskope.ini" ).arg( QCoreApplication::applicationDirPath() ), QSettings::IniFormat );
		cfg.beginGroup( "Settings" );
		NifSkope::SetAppLocale( cfg.value( "Locale", "en" ).toLocale() );
		cfg.endGroup();

		// Load XML files
		NifModel::loadXML();
		KfmModel::loadXML();

		int port = NIFSKOPE_IPC_PORT;

		QStack<QString> fnames;

		// Command Line setup
		QCommandLineParser parser;
		parser.addHelpOption();
		parser.addVersionOption();

		// Add port option
		QCommandLineOption portOption( {"p", "port"}, "Port NifSkope listens on", "port" );
		parser.addOption( portOption );

		// Process options
		parser.process( *a );

		// Override port value
		if ( parser.isSet( portOption ) )
			port = parser.value( portOption ).toInt();

		// Files were passed to NifSkope
		for ( const QString & arg : parser.positionalArguments() ) {
			QString fname = QDir::current().filePath( arg );

			if ( QFileInfo( fname ).exists() ) {
				fnames.push( fname );
			}
		}

		// No files were passed to NifSkope, push empty string
		if ( fnames.isEmpty() ) {
			fnames.push( QString() );
		}

		if ( IPCsocket * ipc = IPCsocket::create( port ) ) {
			//qDebug() << "IPCSocket exec";
			ipc->execCommand( QString( "NifSkope::open %1" ).arg( fnames.pop() ) );

			while ( !fnames.isEmpty() ) {
				IPCsocket::sendCommand( QString( "NifSkope::open %1" ).arg( fnames.pop() ), port );
			}

			return a->exec();
		} else {
			//qDebug() << "IPCSocket send";
			while ( !fnames.isEmpty() ) {
				IPCsocket::sendCommand( QString( "NifSkope::open %1" ).arg( fnames.pop() ), port );
			}
			return 0;
		}
	} else {
		// Future command line batch tools here
	}

	return 0;
}


