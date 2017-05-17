/***** BEGIN LICENSE BLOCK *****

BSD License

Copyright (c) 2005-2015, NIF File Format Library and Tools
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the NIF File Format Library and Tools project may not be
   used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

***** END LICENCE BLOCK *****/

#include "actionmenu.h"

#include "ui/checkablemessagebox.h"

#include <QCache>
#include <QDir>
#include <QSettings>



//! \file actionmenu.cpp ActionMenu implementation

QList<ActionPtr> & ActionMenu::allactions()
{
	static QList<ActionPtr> _actions = QList<ActionPtr>();
	return _actions;
}

QList<ActionMenu *> & ActionMenu::actionmenus()
{
	static QList<ActionMenu *> _menus = QList<ActionMenu *>();
	return _menus;
}

QMultiHash<QString, ActionPtr> & ActionMenu::hash()
{
	static QMultiHash<QString, ActionPtr> _hash = QMultiHash<QString, ActionPtr>();
	return _hash;
}

QList<ActionPtr> & ActionMenu::instants()
{
	static QList<ActionPtr> _instants = QList<ActionPtr>();
	return _instants;
}

QList<ActionPtr> & ActionMenu::sanitizers()
{
	static QList<ActionPtr> _sanitizers = QList<ActionPtr>();
	return _sanitizers;
}

ActionMenu::ActionMenu( NifModel * nif, const QModelIndex & index, QObject * receiver, const char * member ) : QMenu(), Nif( 0 )
{
	setTitle( "Actions" );

	// register this menu in the library
	actionmenus().append( this );

	// attach this menu to the specified nif
	sltNif( nif );

	// fill in the known actions
	for ( ActionPtr a : allactions() ) {
		newActionRegistered( a );
	}

	// set the current index
	sltIndex( index );

	connect( this, &ActionMenu::triggered, this, &ActionMenu::sltActionTriggered );

	if ( receiver && member )
		connect( this, SIGNAL( sigIndex( const QModelIndex & ) ), receiver, member );
}

ActionMenu::~ActionMenu()
{
	actionmenus().removeAll( this );
}

void ActionMenu::cast( NifModel * nif, const QModelIndex & index, ActionPtr a )
{
	QSettings cfg;

	bool suppressConfirm = cfg.value( "Settings/Suppress Undoable Confirmation", false ).toBool();
	bool accepted = false;

	QDialogButtonBox::StandardButton response = QDialogButtonBox::Yes;

	if ( !suppressConfirm ) {
		response = CheckableMessageBox::question( this, "Confirmation", "This action cannot currently be undone. Do you want to continue?", "Do not ask me again", &accepted );

		if ( accepted )
			cfg.setValue( "Settings/Suppress Undoable Confirmation", true );
	}
	
	if ( (response == QDialogButtonBox::Yes) && a && a->isApplicable( nif, index ) ) {
		bool noSignals = a->batch();
		if ( noSignals )
			nif->setState( BaseModel::Processing );
		// Cast the action and return index
		auto idx = a->cast( nif, index );
		if ( noSignals )
			nif->resetState();

		// Refresh the header
		nif->invalidateConditions( nif->getHeader(), true );
		nif->updateHeader();

		if ( noSignals && nif->getProcessingResult() ) {
			emit nif->dataChanged( idx, idx );
		}

		emit sigIndex( idx );
	}
}

void ActionMenu::sltActionTriggered( QAction * action )
{
	ActionPtr a = Map.value( action );
	cast( Nif, Index, a );
}

void ActionMenu::sltNif( NifModel * nif )
{
	if ( Nif )
		disconnect( Nif, &NifModel::modelReset, this, static_cast<void (ActionMenu::*)()>(&ActionMenu::checkActions) );

	Nif = nif;
	Index = QModelIndex();

	if ( Nif )
		connect( Nif, &NifModel::modelReset, this, static_cast<void (ActionMenu::*)()>(&ActionMenu::checkActions) );
}

void ActionMenu::sltIndex( const QModelIndex & index )
{
	if ( index.model() == Nif )
		Index = index;
	else
		Index = QModelIndex();

	checkActions();
}

void ActionMenu::checkActions()
{
	checkActions( this, QString() );
}

void ActionMenu::checkActions( QMenu * menu, const QString & page )
{
	bool menuEnable = false;
	for ( QAction * action : menu->actions() ) {
		if ( action->menu() ) {
			checkActions( action->menu(), action->menu()->title() );
			menuEnable |= action->menu()->isEnabled();
			action->setVisible( action->menu()->isEnabled() );
		} else {
			for ( ActionPtr a : allactions() ) {
				if ( action->text() == a->name() && page == a->page() ) {
					bool actionEnable = Nif && a->isApplicable( Nif, Index );
					action->setVisible( actionEnable );
					action->setEnabled( actionEnable );
					menuEnable |= actionEnable;
				}
			}
		}
	}
	menu->setEnabled( menuEnable );
}

void ActionMenu::newActionRegistered( ActionPtr a )
{
	if ( a->page().isEmpty() ) {
		Map.insert( addAction( a->icon(), a->name() ), a );
	} else {
		QMenu * menu = nullptr;
		for ( QAction * action : actions() ) {
			if ( action->menu() && action->menu()->title() == a->page() ) {
				menu = action->menu();
				break;
			}
		}

		if ( !menu ) {
			menu = new QMenu( a->page(), this );
			addMenu( menu );
		}

		QAction * act = menu->addAction( a->icon(), a->name() );
		act->setShortcut( a->hotkey() );
		Map.insert( act, a );
	}
}

void ActionMenu::registerAction( ActionPtr a )
{
	allactions().append( a );
	hash().insertMulti( a->name(), a );

	if ( a->instant() )
		instants().append( a );

	if ( a->sanity() )
		sanitizers().append( a );

	for ( ActionMenu * m : actionmenus() ) {
		m->newActionRegistered( a );
	}
}

ActionPtr ActionMenu::lookup( const QString & id )
{
	if ( id.isEmpty() )
		return nullptr;

	QString page;
	QString name = id;

	if ( id.contains( "/" ) ) {
		QStringList split = id.split( "/" );
		page = split.value( 0 );
		name = split.value( 1 );
	}

	for ( ActionPtr a : hash().values( name ) ) {
		if ( a->page() == page )
			return a;
	}

	return nullptr;
}

ActionPtr ActionMenu::lookup( const QKeySequence & hotkey )
{
	if ( hotkey.isEmpty() )
		return nullptr;

	for ( ActionPtr a : allactions() ) {
		if ( a->hotkey() == hotkey )
			return a;
	}

	return nullptr;
}

ActionPtr ActionMenu::instant( const NifModel * nif, const QModelIndex & index )
{
	for ( ActionPtr a : instants() ) {
		if ( a->isApplicable( nif, index ) )
			return a;
	}
	return nullptr;
}

QModelIndex ActionMenu::sanitize( NifModel * nif )
{
	QPersistentModelIndex ridx;

	for ( ActionPtr a : sanitizers() ) {
		if ( a->isApplicable( nif, QModelIndex() ) ) {
			QModelIndex idx = a->cast( nif, QModelIndex() );

			if ( idx.isValid() && !ridx.isValid() )
				ridx = idx;
		}
	}

	return ridx;
}

QAction * ActionMenu::exec( const QPoint & pos, QAction * act )
{
	if ( isEnabled() )
		return QMenu::exec( pos, act );

	return nullptr;
}
