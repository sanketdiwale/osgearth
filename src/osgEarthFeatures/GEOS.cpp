/* -*-c++-*- */
/* osgEarth - Dynamic map generation toolkit for OpenSceneGraph
 * Copyright 2008-2009 Pelican Ventures, Inc.
 * http://osgearth.org
 *
 * osgEarth is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 */
#ifdef OSGEARTH_HAVE_GEOS

#include <osgEarthFeatures/GEOS>
#include <osg/Notify>

#include <geos/geom/PrecisionModel.h>
#include <geos/geom/GeometryFactory.h>
#include <geos/geom/Coordinate.h>
#include <geos/geom/CoordinateSequence.h>
#include <geos/geom/CoordinateArraySequenceFactory.h>
#include <geos/geom/Geometry.h>
#include <geos/geom/Point.h>
#include <geos/geom/MultiPoint.h>
#include <geos/geom/Polygon.h>
#include <geos/geom/MultiPolygon.h>
#include <geos/geom/LineString.h>
#include <geos/geom/MultiLineString.h>
#include <geos/geom/LinearRing.h>
#include <geos/operation/buffer/BufferOp.h>

using namespace osgEarth;
using namespace osgEarth::Features;
using namespace geos;
using namespace geos::operation;


static
geom::CoordinateSequence*
vec3dArray2CoordSeq( const osg::Vec3dArray* input, bool close, const geom::CoordinateSequenceFactory* factory )
{   
    std::vector<geos::geom::Coordinate>* coords = new std::vector<geom::Coordinate>();
    coords->reserve( input->size() );
    for( osg::Vec3dArray::const_iterator i = input->begin(); i != input->end(); ++i )
    {
        coords->push_back( geom::Coordinate( i->x(), i->y(), i->z() ));
    }
    if ( close && coords->size() > 2 && (*coords)[0] != (*coords)[coords->size()-1] )
    {
        coords->push_back( (*coords)[0] );
    }
    geom::CoordinateSequence* seq = factory->create( coords );

    return seq;
}

geos::geom::Geometry* 
GEOSUtils::importGeometry( const FeatureGeometry& input, const FeatureProfile* profile )
{
    geom::Geometry* output = 0L;
    geom::PrecisionModel* pm = new geom::PrecisionModel( geom::PrecisionModel::FLOATING );
    const geom::GeometryFactory* factory = new geom::GeometryFactory( pm );

    switch( profile->getGeometryType() )
    {
    case FeatureProfile::GEOM_POINT:
        {
            std::vector<geom::Geometry*>* points = new std::vector<geom::Geometry*>();
            for( FeatureGeometry::const_iterator part_i = input.begin(); part_i != input.end(); ++part_i )
            {
                // every time we find a solid (ccw) call it a new poly.
                osg::Vec3dArray* part = part_i->get();                
                if ( part->size() >= 2 )
                {
                    geom::CoordinateSequence* seq = vec3dArray2CoordSeq( part, false, factory->getCoordinateSequenceFactory() );
                    points->push_back( factory->createPoint( seq ) );
                }
            }
            output = factory->createMultiPoint( points );
        }
        break;

    case FeatureProfile::GEOM_LINE:
        {
            std::vector<geom::Geometry*>* lines = new std::vector<geom::Geometry*>();
            for( FeatureGeometry::const_iterator part_i = input.begin(); part_i != input.end(); ++part_i )
            {
                osg::Vec3dArray* part = part_i->get();                
                if ( part->size() >= 2 )
                {
                    geom::CoordinateSequence* seq = vec3dArray2CoordSeq( part, false, factory->getCoordinateSequenceFactory() );
                    lines->push_back( factory->createLineString( seq ) );
                }
            }
            output = factory->createMultiLineString( lines );
        }
        break;

    case FeatureProfile::GEOM_POLYGON:
        {
            std::vector<geom::Geometry*>* polys = new std::vector<geom::Geometry*>();
            
            geom::LinearRing* shell = 0L;
            std::vector<geom::Geometry*>* holes = 0L;

            // Loop through all the feature parts. Every time we find a new CCW part, start a new 
            // GEOS polygon. Every time we find a CW part, add this to the current poly as a "hole".
            for( FeatureGeometry::const_iterator part_i = input.begin(); part_i != input.end(); ++part_i )
            {
                // every time we find a solid (ccw) call it a new poly.
                osg::Vec3dArray* part = part_i->get();
                
                geom::CoordinateSequence* seq = vec3dArray2CoordSeq( part, true, factory->getCoordinateSequenceFactory() );
                
                if ( FeatureGeometry::isCCW( part ) )
                {
                    if ( shell ) // need a new poly
                    {
                        polys->push_back( factory->createPolygon( shell, holes ) );
                        shell = 0L;
                        holes = 0L;
                    }

                    shell = factory->createLinearRing( seq );
                }
                else
                {
                    if ( !holes )
                         holes = new std::vector<geom::Geometry*>();

                    holes->push_back( factory->createLinearRing( seq ) );
                }
            }

            if ( shell )
            {
                polys->push_back( factory->createPolygon( shell, holes ) );
            }

            output = factory->createMultiPolygon( polys );
        }
        break;
    }

    return output;
}

bool
GEOSUtils::exportGeometry( geom::Geometry* input, FeatureGeometry& output, const FeatureProfile* context )
{
    if ( dynamic_cast<geom::Point*>( input ) )
    {
        osg::notify(osg::NOTICE) << "[osgEarth] GEOS point NYI" << std::endl;        
    }
    else if ( dynamic_cast<geom::MultiPoint*>( input ) )
    {
        geom::MultiPoint* mp = static_cast<geom::MultiPoint*>( input );
        osg::Vec3dArray* part = new osg::Vec3dArray( mp->getNumPoints() );
        for( int i=0; i < mp->getNumPoints(); i++ )
        {
            geom::Point* p = (geom::Point*)(mp->getGeometryN(i));
            part->push_back( osg::Vec3d( p->getX(), p->getY(), 0 ) );            
        }
        output.push_back( part );
    }
    else if ( dynamic_cast<geom::LineString*>( input ) )
    {
        osg::notify(osg::NOTICE) << "[osgEarth] GEOS line string NYI" << std::endl;        
    }
    else if ( dynamic_cast<geom::MultiLineString*>( input ) )
    {
        osg::notify(osg::NOTICE) << "[osgEarth] GEOS multi line string NYI" << std::endl;
    }
    else if ( dynamic_cast<geom::Polygon*>( input ) )
    {
        //osg::notify(osg::NOTICE) << "Polygon" << std::endl;
        geom::Polygon* poly = static_cast<geom::Polygon*>( input );

        output.clear();
        const geom::LineString* outerRing = poly->getExteriorRing();
        if ( outerRing )
        {
            const geom::CoordinateSequence* s = outerRing->getCoordinates();
            osg::Vec3dArray* part = new osg::Vec3dArray( s->getSize() );
            for( int j=0; j<s->getSize(); j++ ) 
            {
                const geom::Coordinate& c = s->getAt( j );
                (*part)[j].set( c.x, c.y, 0 );                
            }
            output.push_back( part );

            for( int k=0; k < poly->getNumInteriorRing(); k++ )
            {
                const geom::LineString* inner = poly->getInteriorRingN( k );
                const geom::CoordinateSequence* s = inner->getCoordinates();
                osg::Vec3dArray* part = new osg::Vec3dArray(s->getSize());
                for( int m = 0; m<s->getSize(); m++ )
                {
                    const geom::Coordinate& c = s->getAt( m );
                    (*part)[m].set( c.x, c.y, 0 );
                }
                output.push_back( part );
            }
        }
    }
    else if ( dynamic_cast<geom::MultiPolygon*>( input ) )
    {
        //osg::notify(osg::NOTICE) << "Multipolygon" << std::endl;
        geom::MultiPolygon* mpoly = static_cast<geom::MultiPolygon*>( input );
        for( int i=0; i<mpoly->getNumGeometries(); i++ )
        {
            const geom::Polygon* poly = static_cast<const geom::Polygon*>( mpoly->getGeometryN(i) );
            const geom::LineString* outerRing = poly->getExteriorRing();
            if ( outerRing )
            {
                geom::CoordinateSequence* s = outerRing->getCoordinates();
                osg::Vec3dArray* part = new osg::Vec3dArray( s->getSize() );
                for( int j=0; j<s->getSize(); j++ ) 
                {
                    const geom::Coordinate& c = s->getAt( j );
                    (*part)[j].set( c.x, c.y, 0 );               
                }
                output.push_back( part );
            }

            for( int k=0; k < poly->getNumInteriorRing(); k++ )
            {
                const geom::LineString* inner = poly->getInteriorRingN( k );
                const geom::CoordinateSequence* s = inner->getCoordinates();
                osg::Vec3dArray* part = new osg::Vec3dArray(s->getSize());
                for( int m = 0; m<s->getSize(); m++ )
                {
                    const geom::Coordinate& c = s->getAt( m );
                    (*part)[m].set( c.x, c.y, 0 );
                }
                output.push_back( part );
            }
        }        
    }
    return true;
}

//bool
//GEOSUtils::buffer(double distance,
//                  int numSegsPer90deg,
//                  const StyleClass& style,
//                  const FeatureGeometry& input,
//                  FeatureGeometry& output,
//                  const FeatureProfile* profile )
//{
//    bool ok = false;
//
//    geom::Geometry* inGeom = importGeometry( input, profile );
//    if ( inGeom )
//    {
//        Stroke& stroke = style.lineSymbolizer().stroke();
//
//        buffer::BufferParameters::EndCapStyle endCap =
//            stroke.lineCap() == Stroke::CAP_
//        geom::Geometry* outGeom = buffer::BufferOp::bufferOp(
//            inGeom,
//            distance,
//            numSegsPer90deg );
//        //    //buffer::OffsetCurveBuilder::DEFAULT_QUADRANT_SEGMENTS,
//        //    //buffer::BufferOp::CAP_::CAP_BUTT ); //:CAP_SQUARE );
//
//        if ( outGeom )
//        {
//            exportGeometry( outGeom, output, profile );
//            outGeom->getFactory()->destroyGeometry( outGeom );
//            //geom::GeometryFactory::getDefaultInstance()->destroyGeometry( outGeom );
//            ok = true;
//        }
//        else
//        {
//            osg::notify(osg::NOTICE) << "[osgEarth] Buffer: no output geometry.." << std::endl;
//            ok = false;
//        }
//
//        inGeom->getFactory()->destroyGeometry( inGeom );
//        //geom::GeometryFactory::getDefaultInstance()->destroyGeometry( inGeom );
//    }
//    else
//    {
//        osg::notify(osg::NOTICE) << "[osgEarth] Buffer: importGeom failed" << std::endl;
//        ok = false;
//    }
//
//    return ok;
//}



#endif // OSGEARTH_HAVE_GEOS

