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
# include <Inventor/nodes/SoSeparator.h>
# include <Inventor/nodes/SoMaterial.h>
# include <Inventor/nodes/SoMaterialBinding.h>
# include <Inventor/nodes/SoCoordinate3.h>
# include <Inventor/nodes/SoDrawStyle.h>
# include <Inventor/nodes/SoMarkerSet.h>
# include <Inventor/SbColor.h>
# include <Inventor/nodes/SoLineSet.h>
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

#include <Inventor/events/SoKeyboardEvent.h>

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

/* XPM */
static const char *cursor_breakline[]={
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
"................##..#...........",
"..................###...........",
".....................##.........",
"................................",
"................................",
"................................",
"............##..................",
"..............###...............",
"..............#..##.............",
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


// BreakLine using multipoint splitLine
class DrawSketchHandlerBreakLine : public DrawSketchHandler
{
public:
    DrawSketchHandlerBreakLine() : Mode(STATUS_START), EditCurve(4), pointOnLine(false), lineId(Constraint::GeoUndef) {}

    // mode table
    enum SelectMode {
	STATUS_START,
	STATUS_SEEK_FIRST,
	STATUS_SEEK_SECOND,
	STATUS_END
    };
    
    virtual ~DrawSketchHandlerBreakLine() {}
    
    virtual void activated(ViewProviderSketch *sketchgui)
    {
	setCursor(QPixmap(cursor_breakline), 7, 7);
	if (!checkSelection()) {
	    sketchgui->purgeHandler(); // no code after this line, Handler get deleted in ViewProvider
	}
	Mode = STATUS_SEEK_FIRST;
	// TODO: proper visualisation of selected points
    }
    
    virtual void mouseMove(Base::Vector2D onSketchPos)
    {
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
	    pointOnLine = false;
	}
	else {
	    pointOnLine = true;
	}
	
	if (Mode == STATUS_START) {
	    Mode = STATUS_SEEK_FIRST;
	}
	else if (Mode == STATUS_SEEK_FIRST) {
	    if (pointOnLine == true) {
		EditCurve[0] = onSketchPos;
		EditCurve[3] = EditCurve[2] = EditCurve[1] = Base::Vector2D(linePos3.x, linePos3.y);
		setPositionText(EditCurve[1]);
		breakPointStart = linePos3;
	    }
	    else {
		EditCurve[0] = Base::Vector2D(0.f,0.f);
		EditCurve[3] = EditCurve[2] = EditCurve[1] = EditCurve[0];
		resetPositionText();
	    }
	}
	else if (Mode == STATUS_SEEK_SECOND) {
	    if (pointOnLine == true) {
		EditCurve[2] = Base::Vector2D(linePos3.x, linePos3.y);
		EditCurve[3] = onSketchPos;
		setPositionText(EditCurve[3]);
		breakPointEnd = linePos3;
	    }
	    else {
		EditCurve[3] = EditCurve[2] = EditCurve[1];
		resetPositionText();
	    }
	}
	sketchgui->drawEdit(EditCurve);
	applyCursor();
	
    }
    
    virtual bool pressButton(Base::Vector2D onSketchPos)
    {
	if (Mode == STATUS_SEEK_FIRST && pointOnLine == true) {
	    Mode = STATUS_SEEK_SECOND;
	}
	else if (Mode == STATUS_SEEK_SECOND && pointOnLine == true) {
	    Mode = STATUS_END;
	}
    }
    
    virtual bool releaseButton(Base::Vector2D onSketchPos)
    {
	if (Mode == STATUS_END) {

	    std::vector<Base::Vector3d > breakPoints;
	    breakPoints.push_back(breakPointStart);
	    breakPoints.push_back(breakPointEnd);
	    
	    // TODO: add a way to prevent constraint addition to the segment being removed
	    Gui::Command::openCommand("Break line");
	    try {
		sketchgui->getSketchObject()->splitLine(lineId, breakPoints);
	    }
	    catch (Base::ValueError e) {
		Base::Console().Message("%s", e.what());
	    }
	    // Segment to be removed should be with the second highest index
	    sketchgui->getSketchObject()->delGeometry(sketchgui->getSketchObject()->getHighestCurveIndex() - 1);
	    Gui::Command::commitCommand();
	    Gui::Command::updateActive();
	    
	    unsetCursor();
	    resetPositionText();
	    EditCurve.clear();
	    sketchgui->drawEdit(EditCurve);
	    Gui::Selection().clearSelection();
	    sketchgui->purgeHandler();
	}
    }
    
    void registerPressedKey(bool pressed, int key)
    {
	if (Mode == STATUS_SEEK_SECOND && key == SoKeyboardEvent::R && !pressed) {
	    Mode = STATUS_SEEK_FIRST;
	    EditCurve[0] = Base::Vector2D(0.f,0.f);
	    EditCurve[3] = EditCurve[2] = EditCurve[1] = EditCurve[0];
	    resetPositionText();
	    sketchgui->drawEdit(EditCurve);
	}
    }

protected:
    
    
    SelectMode Mode;
    std::vector<Base::Vector2D> EditCurve;
    Base::Vector3d startPoint, endPoint, origLineDir, startPointDist, breakPointStart, breakPointEnd;
    int lineId;
    bool pointOnLine;
    
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

DEF_STD_CMD_A(CmdSketcherBreakLine);

CmdSketcherBreakLine::CmdSketcherBreakLine()
    :Command("Sketcher_BreakLine")
{
    sAppModule      = "Sketcher";
    sGroup          = QT_TR_NOOP("Sketcher");
    sMenuText       = QT_TR_NOOP("Break line");
    sToolTipText    = QT_TR_NOOP("Breaks the currently selected line into two gap separated lines");
    sWhatsThis      = sToolTipText;
    sStatusTip      = sToolTipText;
    sPixmap         = "Sketcher_BreakLine";
    sAccel          = "";
    eType           = ForEdit;
}

void CmdSketcherBreakLine::activated(int iMsg)
{
    ActivateHandler(getActiveGuiDocument(), new DrawSketchHandlerBreakLine());
}

bool CmdSketcherBreakLine::isActive(void)
{
    return isAlterGeoActive( getActiveGuiDocument() );
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

static const char *cursor_splitline_ok[]={
"32 32 3 1",
"+ c white",
"# c green",
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

// Multipoint splitline
class DrawSketchHandlerSplitLine: public DrawSketchHandler
{

public:
    DrawSketchHandlerSplitLine() : Mode(STATUS_START), pointOnLine(false), lineId(Constraint::GeoUndef), splitPoints(0),  okCursor(QPixmap(cursor_splitline_ok), 7, 7) {}

    // mode table
    enum SelectMode {
	STATUS_START,
	STATUS_SEEK_POINT,
	STATUS_END
    };
    
    virtual ~DrawSketchHandlerSplitLine() {}
    
    virtual void activated(ViewProviderSketch *sketchgui)
    {
	setCursor(QPixmap(cursor_splitline), 7, 7);
	if (!checkSelection()) {
	    sketchgui->purgeHandler(); // no code after this line, Handler get deleted in ViewProvider
	}
	Mode = STATUS_SEEK_POINT;
	
	// create nodes for split point visualization
	SoSeparator * pointsRoot = new SoSeparator;
	getHandlerRoot()->addChild(pointsRoot);
	
	SplitPointsMaterials = new SoMaterial;
	SplitPointsMaterials->setName("SplitPointsMaterials");
	pointsRoot->addChild(SplitPointsMaterials);

	SoMaterialBinding *MtlBind = new SoMaterialBinding;
	MtlBind->setName("SplitPointsMaterialBinding");
	MtlBind->value = SoMaterialBinding::PER_VERTEX;
	pointsRoot->addChild(MtlBind);

	SplitPointsCoordinate = new SoCoordinate3;
	SplitPointsCoordinate->setName("SplitPointsCoordinate");
	pointsRoot->addChild(SplitPointsCoordinate);

	SoDrawStyle *DrawStyle = new SoDrawStyle;
	DrawStyle->setName("SplitPointsDrawStyle");
	DrawStyle->pointSize = 8;
	pointsRoot->addChild(DrawStyle);

	SoMarkerSet * PointSet = new SoMarkerSet;
	PointSet->setName("PointSet");
	PointSet->markerIndex = SoMarkerSet::CIRCLE_FILLED_7_7;
	pointsRoot->addChild(PointSet);
	
	// Splitter line
	SoSeparator * SplitterLineRoot = new SoSeparator;
	getHandlerRoot()->addChild(SplitterLineRoot);
	
	SplitterLineMaterial = new SoMaterial;
	SplitterLineMaterial->setName("SplitterLineMaterial");
	SplitterLineRoot->addChild(SplitterLineMaterial);
	
	SplitterLineCoordinate = new SoCoordinate3;
	SplitterLineCoordinate->setName("SplitterLineCoordinate");
	SplitterLineCoordinate->point.setNum(2);
	SplitterLineRoot->addChild(SplitterLineCoordinate);
	
	DrawStyle = new SoDrawStyle;
	DrawStyle->setName("SplitterLineDrawStyle");
	DrawStyle->lineWidth = 3;
	DrawStyle->linePattern = 0xf0f0;
	SplitterLineRoot->addChild(DrawStyle);
	
	SplitterLineSet = new SoLineSet;
	SplitterLineSet->setName("SplitterLineSet");
	SplitterLineSet->numVertices.setNum(1);
	SplitterLineRoot->addChild(SplitterLineSet);
	
	zHandler = getzHandler();
	SplitPointColor.setValue(0,1,0);
    }
    
    virtual void mouseMove(Base::Vector2D onSketchPos)
    {
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
	    pointOnLine = false;
	    applyCursor(okCursor);
	}
	else {
	    pointOnLine = true;
	    applyCursor();
	}
	
	if (Mode == STATUS_SEEK_POINT) {
	    if (pointOnLine == true) {
		setPositionText(Base::Vector2D(linePos3.x, linePos3.y));
		SplitterLineSet->numVertices.setNum(1);
		SplitterLineCoordinate->point.setNum(2);
		SbVec3f * verts = SplitterLineCoordinate->point.startEditing();
		int32_t * index = SplitterLineSet->numVertices.startEditing();
		verts[0].setValue(onSketchPos.fX, onSketchPos.fY, zHandler);
		verts[1].setValue(linePos3.x, linePos3.y, zHandler);
		index[0] = 2;
		SplitterLineCoordinate->point.finishEditing();
		SplitterLineSet->numVertices.finishEditing();
		currentSplitPoint = linePos3;
	    }
	    else {

		SbVec3f * verts = SplitterLineCoordinate->point.startEditing();
		SplitterLineSet->numVertices.startEditing();
		verts[0].setValue(0,0,0);
		verts[1].setValue(0,0,0);
		SplitterLineCoordinate->point.finishEditing();
		SplitterLineSet->numVertices.finishEditing();
		setPositionText(onSketchPos, "OK");
	    }
	}
    }
    
    virtual bool pressButton(Base::Vector2D onSketchPos)
    {
	if (Mode == STATUS_SEEK_POINT && pointOnLine == true) {
	    splitPoints.push_back(currentSplitPoint);
	    
	    SplitPointsCoordinate->point.setNum(splitPoints.size());
	    SbVec3f * pverts = SplitPointsCoordinate->point.startEditing();
	    pverts[splitPoints.size()-1].setValue(currentSplitPoint.x, currentSplitPoint.y, zHandler);
	    SplitPointsCoordinate->point.finishEditing();
	    
	    SplitPointsMaterials->diffuseColor.setNum(splitPoints.size());
	    SbColor * pcolor = SplitPointsMaterials->diffuseColor.startEditing();
	    pcolor[splitPoints.size()-1] = SplitPointColor;
	    SplitPointsMaterials->diffuseColor.finishEditing();
	}
	else if(Mode == STATUS_SEEK_POINT && pointOnLine == false) {
	    Mode = STATUS_END;
	}
    }
    
    virtual bool releaseButton(Base::Vector2D onSketchPos)
    {
	if (Mode != STATUS_END) {
	    if (pointOnLine) {
		applyCursor();
	    }
	    else {
		applyCursor(okCursor);
	    }
	}
	else {
	    Gui::Command::openCommand("Split line");
	    try {
		sketchgui->getSketchObject()->splitLine(lineId, splitPoints);
	    }
	    catch (Base::ValueError e) {
		Base::Console().Message("%s", e.what());
	    }
	    Gui::Command::commitCommand();
	    Gui::Command::updateActive();
	    
	    unsetCursor();
	    resetPositionText();
	    Gui::Selection().clearSelection();
	    sketchgui->purgeHandler();
	}
    }
    
    void registerPressedKey(bool pressed, int key)
    {
	switch (key)
	{
	    case SoKeyboardEvent::ENTER:
		// Finish point selection
		if (!pressed) {
		    Mode = STATUS_END;
		    releaseButton(Base::Vector2D(0.f, 0.f));
		}
		break;
	    case SoKeyboardEvent::R:
		// Remove the previously added point
		if (!pressed && !splitPoints.empty()) {
		    splitPoints.pop_back();
		    SplitPointsCoordinate->point.setNum(splitPoints.size());
		    SplitPointsMaterials->diffuseColor.setNum(splitPoints.size());
		}
		break;
	}
    }    
    
protected:
    SelectMode Mode;
    QCursor okCursor;
    bool pointOnLine;
    int lineId;
    Base::Vector3d startPoint, endPoint, origLineDir, startPointDist;
    std::vector<Base::Vector3d> splitPoints;
    Base::Vector3d currentSplitPoint;
    float zHandler;
    SoCoordinate3 * SplitPointsCoordinate;
    SbColor SplitPointColor;
    SoMaterial * SplitPointsMaterials;
    SoMaterial * SplitterLineMaterial;
    SoCoordinate3 * SplitterLineCoordinate;
    SoLineSet * SplitterLineSet;
    
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
    sToolTipText    = QT_TR_NOOP("Splits the currently selected line into several connected lines. Enter finishes selection, 'r' removes previous point");
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
    if (isAlterGeoActive( getActiveGuiDocument() ))
    {
	std::vector<Gui::SelectionObject> selection = Gui::Selection().getSelectionEx();
	if (selection.size() != 1) return false;
	
	const std::vector<std::string> &SubNames = selection[0].getSubNames();
	if (SubNames.size() != 1) return false;
	
	Sketcher::SketchObject* Obj = dynamic_cast<Sketcher::SketchObject*>(selection[0].getObject());
	int GeoId1;
	Sketcher::PointPos PosId1;
	getIdsFromName(SubNames[0], Obj, GeoId1, PosId1);
	
	// selection/undo bug elsewhere, or is this problematic?
	//if (!isEdge(GeoId1, PosId1) || GeoId1 < 0 || Obj->getGeometry(GeoId1)->getTypeId() != Part::GeomLineSegment::getClassTypeId()) return false;
	
	return true;
	
    }
    return false;
}


}


void CreateSketcherCommandsAlterGeo(void)
{
    Gui::CommandManager &rcCmdMgr = Gui::Application::Instance->commandManager();

    rcCmdMgr.addCommand(new CmdSketcherToggleConstruction());
    rcCmdMgr.addCommand(new CmdSketcherBreakLine());
    rcCmdMgr.addCommand(new CmdSketcherSplitLine());
}

