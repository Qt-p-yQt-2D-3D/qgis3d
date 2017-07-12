
#include <QApplication>
#include <QBoxLayout>

#include <Qt3DRender>
#include <Qt3DExtras>

#include "maptexturegenerator.h"
#include "sidepanel.h"
#include "window3d.h"
#include "map3d.h"
#include "flatterraingenerator.h"
#include "demterraingenerator.h"
#include "quantizedmeshterraingenerator.h"

#include <qgsapplication.h>
#include <qgsmapsettings.h>
#include <qgsrasterlayer.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>



static QgsRectangle _fullExtent(const QList<QgsMapLayer*>& layers, const QgsCoordinateReferenceSystem& crs)
{
  QgsMapSettings ms;
  ms.setLayers(layers);
  ms.setDestinationCrs(crs);
  return ms.fullExtent();
}

int main(int argc, char *argv[])
{
  // TODO: why it does not work to create ordinary QApplication and then just call initQgis()

  qputenv("QGIS_PREFIX_PATH", "/home/martin/qgis/git-master/build59/output");
  QgsApplication app(argc, argv, true);
  QgsApplication::initQgis();

  QgsRasterLayer* rlDtm = new QgsRasterLayer("/home/martin/tmp/qgis3d/dtm.tif", "dtm", "gdal");
  Q_ASSERT( rlDtm->isValid() );

  QgsRasterLayer* rlSat = new QgsRasterLayer("/home/martin/tmp/qgis3d/ap.tif", "ap", "gdal");
  Q_ASSERT( rlSat->isValid() );

  QgsVectorLayer* vlPolygons = new QgsVectorLayer("/home/martin/tmp/qgis3d/osm.db|layerid=3", "buildings", "ogr");
  Q_ASSERT( vlPolygons->isValid() );

  Map3D map;
  map.layers << rlSat;
  map.crs = rlSat->crs();
  map.zExaggeration = 3;
  map.originZ = 0;

  map.tileTextureSize = 512;

  TerrainGenerator::Type tt;
  tt = TerrainGenerator::Flat;
  //tt = TerrainGenerator::Dem;
  //tt = TerrainGenerator::QuantizedMesh;

  if (tt == TerrainGenerator::Flat)
  {
    // TODO: tiling scheme - from this project's CRS + full extent
    FlatTerrainGenerator* flatTerrain = new FlatTerrainGenerator;
    map.terrainGenerator.reset(flatTerrain);
  }
  else if (tt == TerrainGenerator::Dem)
  {
    DemTerrainGenerator* demTerrain = new DemTerrainGenerator(rlDtm, 16);
    map.terrainGenerator.reset(demTerrain);
  }
  else if (tt == TerrainGenerator::QuantizedMesh)
  {
    QuantizedMeshTerrainGenerator* qmTerrain = new QuantizedMeshTerrainGenerator;
    map.terrainGenerator.reset(qmTerrain);
  }

  Q_ASSERT(map.terrainGenerator);  // we need a terrain generator

  if (map.terrainGenerator->type() == TerrainGenerator::Flat)
  {
    // we are free to define terrain extent to whatever works best
    static_cast<FlatTerrainGenerator*>(map.terrainGenerator.get())->setExtent(_fullExtent(map.layers, map.crs), map.crs);
  }

  map.ctTerrainToMap = QgsCoordinateTransform(map.terrainGenerator->crs(), map.crs);
  QgsRectangle fullExtentInTerrainCrs = _fullExtent(map.layers, map.terrainGenerator->crs());

  if (map.terrainGenerator->type() == TerrainGenerator::QuantizedMesh)
  {
    // define base terrain tile coordinates
    static_cast<QuantizedMeshTerrainGenerator*>(map.terrainGenerator.get())->setBaseTileFromExtent(fullExtentInTerrainCrs);
  }

  // origin X,Y - at the project extent's center
  QgsPointXY centerTerrainCrs = fullExtentInTerrainCrs.center();
  QgsPointXY centerMapCrs = QgsCoordinateTransform(map.terrainGenerator->terrainTilingScheme.crs, map.crs).transform(centerTerrainCrs);
  map.originX = centerMapCrs.x();
  map.originY = centerMapCrs.y();

  map.mapGen = new MapTextureGenerator(map);

  // polygons

  PolygonRenderer pr;
  pr.layer = vlPolygons;
  pr.ambientColor = Qt::gray;
  pr.diffuseColor = Qt::lightGray;
  pr.height = 0;
  pr.extrusionHeight = 10;
  map.polygonRenderers << pr;

  SidePanel* sidePanel = new SidePanel;
  sidePanel->setMinimumWidth(150);

  Window3D* view = new Window3D(sidePanel, map);
  QWidget *container = QWidget::createWindowContainer(view);

  QSize screenSize = view->screen()->size();
  container->setMinimumSize(QSize(200, 100));
  container->setMaximumSize(screenSize);

  QWidget widget;
  QHBoxLayout *hLayout = new QHBoxLayout(&widget);
  hLayout->setMargin(0);
  hLayout->addWidget(container, 1);
  hLayout->addWidget(sidePanel);

  widget.resize(800,600);
  widget.show();

  return app.exec();
}
