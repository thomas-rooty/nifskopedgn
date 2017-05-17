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

#ifndef ACTIONMENU_H
#define ACTIONMENU_H

#include "message.h"
#include "nifmodel.h"

#include <QMenu> // Inherited
#include <QCoreApplication>
#include <QHash>
#include <QIcon>
#include <QKeySequence>
#include <QList>
#include <QMap>
#include <QPersistentModelIndex>
#include <QString>

#include <memory>


using QIconPtr = std::shared_ptr<QIcon>;

//! \file actionmenu.h Action, ActionMenu and ActionFactory

//! Register an Action using an ActionFactory
#define REGISTER_ACTION( ACTION ) static ActionFactory __ ## ACTION ## __( new ACTION );

//! Flexible context menu magic functions.
class Action
{
public:
	//! Constructor
	Action() {}
	//! Destructor
	virtual ~Action() {}

	//! Name of action
	virtual QString name() const = 0;
	//! Context sub-menu that the action appears on
	virtual QString page() const { return QString(); }
	//! Unused?
	virtual QString hint() const { return QString(); }
	//! Icon displayed in block view
	virtual QIcon icon() const { return QIcon(); }
	//! Whether the action shows up in block list instead of a context menu
	virtual bool instant() const { return false; }
	//! Whether the action performs a sanitizing function
	virtual bool sanity() const { return false; }
	//! Whether the action has a high processing cost
	virtual bool batch() const { return (page() == "Batch") || (page() == "Block") || (page() == "Mesh"); }
	//! Hotkey sequence
	virtual QKeySequence hotkey() const { return QKeySequence(); }

	//! Determine if/when the action can be cast
	virtual bool isApplicable( const NifModel * nif, const QModelIndex & index ) = 0;

	//! Cast (apply) the action
	virtual QModelIndex cast( NifModel * nif, const QModelIndex & index ) = 0;

	//! Cast the action if applicable
	void castIfApplicable( NifModel * nif, const QModelIndex & index )
	{
		if ( isApplicable( nif, index ) )
			cast( nif, index );
	}

	//! i18n wrapper for various strings
	/*!
	 * Note that we don't use QObject::tr() because that doesn't provide
	 * context. We also don't use the QCoreApplication Q_DECLARE_TR_FUNCTIONS()
	 * macro because that won't document properly.
	 *
	 * No actions should reimplement this function.
	 */
	static inline QString tr( const char * key, const char * comment = 0 ) { return QCoreApplication::translate( "Action", key, comment ); }
};

using ActionPtr = std::shared_ptr<Action>;

//! Action menu
class ActionMenu final : public QMenu
{
	Q_OBJECT

public:
	//! Constructor
	ActionMenu( NifModel * nif, const QModelIndex & index = QModelIndex(), QObject * receiver = 0, const char * member = 0 );
	//! Destructor
	~ActionMenu();

	//! From QMenu: Pops up the menu so that the action <i>act</i> will be at the specified global position <i>pos</i>
	QAction * exec( const QPoint & pos, QAction * act = 0 );

	//! Register action with appropriate actions menu
	static void registerAction( ActionPtr action );

	//! Locate action by name
	static ActionPtr lookup( const QString & id );
	//! Locate action by hotkey
	static ActionPtr lookup( const QKeySequence & hotkey );
	//! Locate instant actions by datatype
	static ActionPtr instant( const NifModel * nif, const QModelIndex & index );

	//! Cast all sanitizing actions
	static QModelIndex sanitize( NifModel * nif );

public slots:
	void sltNif( NifModel * nif );

	void sltIndex( const QModelIndex & index );

	void cast( NifModel * nif, const QModelIndex & index, ActionPtr a );

	void checkActions();

signals:
	void sigIndex( const QModelIndex & index );

protected slots:
	void sltActionTriggered( QAction * action );

protected:
	NifModel * Nif;
	QPersistentModelIndex Index;
	QMap<QAction *, ActionPtr> Map;

	void newActionRegistered( ActionPtr a );
	void checkActions( QMenu * menu, const QString & page );

private:
	static QList<ActionPtr> & allactions();
	static QList<ActionMenu *> & actionmenus();
	static QMultiHash<QString, ActionPtr> & hash();
	static QList<ActionPtr> & instants();
	static QList<ActionPtr> & sanitizers();
};

//! ActionMenu manager
class ActionFactory final
{
public:
	//! Contructor.
	/**
	 * Registers the action with the appropriate action menu.
	 *
	 * \param action The action to manage
	 */
	ActionFactory( Action * action )
	{
		ActionMenu::registerAction( ActionPtr( action ) );
	}
};

#endif
