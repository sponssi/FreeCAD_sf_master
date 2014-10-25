/***************************************************************************
 *   Copyright (c) 2011 J�rgen Riegel (juergen.riegel@web.de)              *
 *                                                                         *
 *   This file is part of the FreeCAD CAx development system.              *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Library General Public           *
 *   License as published by the Free Software Foundation; either          *
 *   version 2 of the License, or (at your option) any later version.      *
 *                                                                         *
 *   This library  is distributed in the hope that it will be useful,      *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Library General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this library; see the file COPYING.LIB. If not,    *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,         *
 *   Suite 330, Boston, MA  02111-1307, USA                                *
 *                                                                         *
 ***************************************************************************/


#include "PreCompiled.h"
#ifndef _PreComp_
# include <QMessageBox>
#endif

#include <Gui/Application.h>
#include <Gui/Document.h>
#include <Gui/Selection.h>
#include <Gui/Command.h>
#include <Gui/MainWindow.h>
#include <Gui/DlgEditFileIncludeProptertyExternal.h>

#include <Mod/Part/App/Geometry.h>
#include <Mod/Sketcher/App/SketchObject.h>

#include "ViewProviderSketch.h"
#include "DrawSketchHandler.h"

#include <QCursor>
#include <QPixmap>

using namespace std;
using namespace SketcherGui;
using namespace Sketcher;

bool isAlterGeoActive(Gui::Document *doc)
{
    if (doc)
        // checks if a Sketch Viewprovider is in Edit and is in no special mode
        if (doc->getInEdit() && doc->getInEdit()->isDerivedFrom(SketcherGui::ViewProviderSketch::getClassTypeId()))
            if (dynamic_cast<SketcherGui::ViewProviderSketch*>(doc->getInEdit())
                ->getSketchMode() == ViewProviderSketch::STATUS_NONE)
                if (Gui::Selection().countObjectsOfType(Sketcher::SketchObject::getClassTypeId()) > 0)
                    return true;
    return false;
}

void getIdsFromName(const std::string &name, const Sketcher::SketchObject* Obj,
                    int &GeoId, PointPos &PosId);

bool isEdge(int GeoId, PointPos PosId);

void ActivateHandler(Gui::Document *doc,DrawSketchHandler *handler);
/*{
    if (doc) {
        if (doc->getInEdit() && doc->getInEdit()->isDerivedFrom
            (SketcherGui::ViewProviderSketch::getClassTypeId()))
            dynamic_cast<SketcherGui::ViewProviderSketch*>
            (doc->getInEdit())->activateHandler(handler);
    }
}*/

namespace SketcherGui {


/* Constrain commands =======================================================*/
DEF_STD_CMD_A(CmdSketcherToggleConstruction);

CmdSketcherToggleConstruction::CmdSketcherToggleConstruction()
    :Command("Sketcher_ToggleConstruction")
{
    sAppModule      = "Sketcher";
    sGroup          = QT_TR_NOOP("Sketcher");
    sMenuText       = QT_TR_NOOP("Toggle construction line");
    sToolTipText    = QT_TR_NOOP("Toggles the currently selected lines to/from construction mode");
    sWhatsThis      = "Sketcher_ToggleConstruction";
    sStatusTip      = sToolTipText;
    sPixmap         = "Sketcher_AlterConstruction";
    sAccel          = "T";
    eType           = ForEdit;
}

void CmdSketcherToggleConstruction::activated(int iMsg)
{
    // get the selection
    std::vector<Gui::SelectionObject> selection = getSelection().getSelectionEx();

    // only one sketch with its subelements are allowed to be selected
    if (selection.size() != 1) {
        QMessageBox::warning(Gui::getMainWindow(), QObject::tr("Wrong selection"),
            QObject::tr("Select edge(s) from the sketch."));
        return;
    }

    // get the needed lists and objects
    const std::vector<std::string> &SubNames = selection[0].getSubNames();

    // undo command open
    openCommand("toggle draft from/to draft");

    // go through the selected subelements
    for(std::vector<std::string>::const_iterator it=SubNames.begin();it!=SubNames.end();++it){
        // only handle edges
        if (it->size() > 4 && it->substr(0,4) == "Edge") {
            int GeoId = std::atoi(it->substr(4,4000).c_str()) - 1;
            // issue the actual commands to toggle
            doCommand(Doc,"App.ActiveDocument.%s.toggleConstruction(%d) ",selection[0].getFeatName(),GeoId);
        }
    }
    // finish the transaction and update
    commitCommand();
    updateActive();

    // clear the selection (convenience)
    getSelection().clearSelection();
}

bool CmdSketcherToggleConstruction::isActive(void)
{
    return isAlterGeoActive( getActiveGuiDocument() );
}

/* Break line =======================================================*/
DEF_STD_CMD_A(CmdSketcherBreakLine);

CmdSketcherBreakLine::CmdSketcherBreakLine()
    :Command("Sketcher_BreakLine")
{
    sAppModule      = "Sketcher";
    sGroup          = QT_TR_NOOP("Sketcher");
    sMenuText       = QT_TR_NOOP("Break line");
    sToolTipText    = QT_TR_NOOP("Breakes the currently selected line into two gap separated lines");
    sWhatsThis      = sToolTipText;
    sStatusTip      = sToolTipText;
    sPixmap         = "Sketcher_BreakLine";
    sAccel          = "";
    eType           = ForEdit;
}

void CmdSketcherBreakLine::activated(int iMsg)
{
    QMessageBox::warning(Gui::getMainWindow(), QObject::tr("Not implemented"),
        QObject::tr("Command not implemented yet, please try Sketcher_SplitPoint."));
}

bool CmdSketcherBreakLine::isActive(void)
{
    //return isAlterGeoActive( getActiveGuiDocument() );
    return false;
}

/* Split Line =======================================================*/

/* XPM */
static const char *cursor_splitline[]={
"32 32 3 1",
"+ c white",
"# c red",
". c None",
"......+.........................",
"......+.........................",
"......+.........................",
"......+.........................",
"......+.........................",
"................................",
"+++++...+++++...................",
"................................",
"......+...............###.......",
"......+...............#.#.......",
"......+...............###.......",
"......+..............#..........",
"......+.............#...........",
"....................#...........",
"...................#............",
"..................#.............",
"................###.............",
"................#.#.............",
"................###.............",
"................#...............",
"...............#................",
"..............#.................",
"..............#.................",
".............#..................",
"..........###...................",
"..........#.#...................",
"..........###...................",
"................................",
"................................",
"................................",
"................................",
"................................"};

class DrawSketchHandlerSplitLine: public DrawSketchHandler
{
public:
    DrawSketchHandlerSplitLine() : Mode(STATUS_START), EditCurve(2) {}
    
    // mode table
    enum SelectMode {
	STATUS_START,
	STATUS_NOT_ON_LINE,
	STATUS_POINT_OK,
	STATUS_END
    };
    
    virtual ~DrawSketchHandlerSplitLine() {}
    
    virtual void activated(ViewProviderSketch *sketchgui)
    {
	setCursor(QPixmap(cursor_splitline), 7, 7);
	if (!checkSelection()) {
	    sketchgui->purgeHandler(); // no code after this line, Handler get deleted in ViewProvider
	}
    }
    
    virtual void mouseMove(Base::Vector2D onSketchPos)
    {
	if (Mode != STATUS_END) {
	    
	    // Find the projection of onSketchPoint on the original line
	    Base::Vector3d onSketchPos3(onSketchPos.fX, onSketchPos.fY, 0);
	    Base::Vector3d linePosDelta = onSketchPos3;
	    linePosDelta.ProjToLine(onSketchPos3, origLineDir);
	    Base::Vector3d linePos3 = onSketchPos3 - startPointDist + linePosDelta;
	    
	    // Check that linePos3 is between the endpoints
	    Base::Vector3d startVec = linePos3 - startPoint;
	    Base::Vector3d endVec = linePos3 - endPoint;
	    
	    if (startVec * origLineDir <= 0 || endVec * origLineDir >= 0) {
		// Point is not on the line
		EditCurve[0] = Base::Vector2D(0.f,0.f);
		EditCurve[1] = Base::Vector2D(0.f,0.f);
		resetPositionText();
		Mode = STATUS_NOT_ON_LINE;
	    }
	    else {
		EditCurve[0] = onSketchPos;
		EditCurve[1] = Base::Vector2D(linePos3.x, linePos3.y);
		setPositionText(EditCurve[1]);
		splitPoint = linePos3;
		Mode = STATUS_POINT_OK;
	    }
	    sketchgui->drawEdit(EditCurve);
	    applyCursor();
	}
    }
    
    virtual bool pressButton(Base::Vector2D onSketchPos)
    {
	if (Mode == STATUS_POINT_OK) {
	    Mode = STATUS_END;
	}
	
	return true;
    }
    
    virtual bool releaseButton(Base::Vector2D onSketchPos)
    {

	if (Mode == STATUS_END) {
	    Gui::Command::openCommand("Split line");
	    sketchgui->getSketchObject()->splitLine(lineId, splitPoint);
	    Gui::Command::commitCommand();
	    Gui::Command::updateActive();
	    unsetCursor();
	    resetPositionText();
	    EditCurve.clear();
	    sketchgui->drawEdit(EditCurve);
	    Gui::Selection().clearSelection();
	    sketchgui->purgeHandler(); // no code after this line, Handler get deleted in ViewProvider
	}
	return true;
    }
    
protected:
    SelectMode Mode;
    std::vector<Base::Vector2D> EditCurve;
    Base::Vector3d startPoint, endPoint, origLineDir, startPointDist, splitPoint;
    int lineId;
    
    bool checkSelection()
    {
        // get the selection
	std::vector<Gui::SelectionObject> selection = Gui::Selection().getSelectionEx();
    
	// only one sketch with its subelements are allowed to be selected
	if (selection.size() != 1) {
	    QMessageBox::warning(Gui::getMainWindow(), QObject::tr("Wrong selection"),
		QObject::tr("Select a line from the sketch."));
	    return false;
	}
    
	// get the needed lists and objects
	const std::vector<std::string> &SubNames = selection[0].getSubNames();
	Sketcher::SketchObject* Obj = dynamic_cast<Sketcher::SketchObject*>(selection[0].getObject());
    
	// Check that only one item is selected
	if (SubNames.size() != 1) {
	    QMessageBox::warning(Gui::getMainWindow(), QObject::tr("Wrong selection"),
		QObject::tr("Select exactly one line from the sketch"));
	    return false;
	}
    
	int GeoId1;
	Sketcher::PointPos PosId1;
	getIdsFromName(SubNames[0], Obj, GeoId1, PosId1);
    
	// Check that the line is an edge
	if (!isEdge(GeoId1, PosId1)) {
	    QMessageBox::warning(Gui::getMainWindow(), QObject::tr("Wrong selection"),
		QObject::tr("Select exactly one line from the sketch"));
	    return false;
	}
    
	// Check that the line is not external
	if (GeoId1 < 0) {
	    QMessageBox::warning(Gui::getMainWindow(), QObject::tr("Wrong selection"),
            QObject::tr("External or reference geometry selected."));
	    return false;
	}
   
	const Part::Geometry * geom = Obj->getGeometry(GeoId1);
	if (geom->getTypeId() != Part::GeomLineSegment::getClassTypeId()) {
	    QMessageBox::warning(Gui::getMainWindow(), QObject::tr("Wrong selection"),
            QObject::tr("Selected edge is not a line."));
	    return false;
	}
	
	const Part::GeomLineSegment *lineSeg;
	lineSeg = dynamic_cast<const Part::GeomLineSegment*>(geom);
	startPoint = lineSeg->getStartPoint();
	endPoint = lineSeg->getEndPoint();
	
	origLineDir = endPoint - startPoint;
	startPointDist = startPoint;
	startPointDist.ProjToLine(startPoint, origLineDir);
		
	lineId = GeoId1;
	
	return true;
    }
};

DEF_STD_CMD_A(CmdSketcherSplitLine);

CmdSketcherSplitLine::CmdSketcherSplitLine()
    :Command("Sketcher_SplitLine")
{
    sAppModule      = "Sketcher";
    sGroup          = QT_TR_NOOP("Sketcher");
    sMenuText       = QT_TR_NOOP("Split line");
    sToolTipText    = QT_TR_NOOP("Splits the currently selected line into two connected lines");
    sWhatsThis      = sToolTipText;
    sStatusTip      = sToolTipText;
    sPixmap         = "Sketcher_SplitLine";
    sAccel          = "";
    eType           = ForEdit;
}

void CmdSketcherSplitLine::activated(int iMsg)
{
    ActivateHandler(getActiveGuiDocument(), new DrawSketchHandlerSplitLine());
}

bool CmdSketcherSplitLine::isActive(void)
{
    return isAlterGeoActive( getActiveGuiDocument() );
}


}


void CreateSketcherCommandsAlterGeo(void)
{
    Gui::CommandManager &rcCmdMgr = Gui::Application::Instance->commandManager();

    rcCmdMgr.addCommand(new CmdSketcherToggleConstruction());
    rcCmdMgr.addCommand(new CmdSketcherBreakLine());
    rcCmdMgr.addCommand(new CmdSketcherSplitLine());
}

