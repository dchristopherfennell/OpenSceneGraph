/* -*-c++-*- OpenSceneGraph - Copyright (C) 1998-2006 Robert Osfield 
 *
 * This library is open source and may be redistributed and/or modified under  
 * the terms of the OpenSceneGraph Public License (OSGPL) version 0.0 or 
 * (at your option) any later version.  The full license is in LICENSE file
 * included with this distribution, and on the openscenegraph.org website.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 * OpenSceneGraph Public License for more details.
*/


#include <osgUtil/PlaneIntersector>

#include <osg/Geometry>
#include <osg/Notify>
#include <osg/io_utils>
#include <osg/TriangleFunctor>

using namespace osgUtil;

namespace PlaneIntersectorUtils
{

    struct RefPolyline : public osg::Referenced
    {
        typedef std::vector<osg::Vec3d> Polyline;
        Polyline _polyline;
        
        void reverse()
        {
            unsigned int s=0;
            unsigned int e=_polyline.size()-1;
            for(; s<e; ++s,--e)
            {
                std::swap(_polyline[s],_polyline[e]);
            }
            
        }
    };

    class PolylineConnector
    {
    public:
        
        typedef std::map<osg::Vec3d, osg::ref_ptr<RefPolyline> > PolylineMap;
        typedef std::vector< osg::ref_ptr<RefPolyline> > PolylineList;
        
        PolylineList    _polylines;
        PolylineMap     _startPolylineMap;
        PolylineMap     _endPolylineMap;
        

        void add(const osg::Vec3d& v1, const osg::Vec3d& v2)
        {
            if (v1==v2) return;
        
            PolylineMap::iterator v1_start_itr = _startPolylineMap.find(v1);
            PolylineMap::iterator v1_end_itr = _endPolylineMap.find(v1);

            PolylineMap::iterator v2_start_itr = _startPolylineMap.find(v2);
            PolylineMap::iterator v2_end_itr = _endPolylineMap.find(v2);

            unsigned int v1_connections = 0;
            if (v1_start_itr != _startPolylineMap.end()) ++v1_connections;
            if (v1_end_itr != _endPolylineMap.end()) ++v1_connections;

            unsigned int v2_connections = 0;
            if (v2_start_itr != _startPolylineMap.end()) ++v2_connections;
            if (v2_end_itr != _endPolylineMap.end()) ++v2_connections;
            
            if (v1_connections==0) // v1 is no connected to anything.
            {
                if (v2_connections==0)
                {
                    // new polyline
                    newline(v1,v2);
                }
                else if (v2_connections==1)
                {
                    // v2 must connect to either a start or an end.
                    if (v2_start_itr != _startPolylineMap.end())
                    {
                        insertAtStart(v1, v2_start_itr);
                    }
                    else if (v2_end_itr != _endPolylineMap.end())
                    {
                        insertAtEnd(v1, v2_end_itr);
                    }
                    else
                    {
                        osg::notify(osg::NOTICE)<<"Error: should not get here!"<<std::endl;
                    }
                }
                else // if (v2_connections==2)
                {
                    // v2 connects to a start and an end - must have a loop in the list!
                    osg::notify(osg::NOTICE)<<"v2="<<v2<<" must connect to a start and an end - must have a loop!!!!!."<<std::endl;
                }
            }
            else if (v2_connections==0) // v1 is no connected to anything.
            {
                if (v1_connections==1)
                {
                    // v1 must connect to either a start or an end.
                    if (v1_start_itr != _startPolylineMap.end())
                    {
                        insertAtStart(v2, v1_start_itr);
                    }
                    else if (v1_end_itr != _endPolylineMap.end())
                    {
                        insertAtEnd(v2, v1_end_itr);
                    }
                    else
                    {
                        osg::notify(osg::NOTICE)<<"Error: should not get here!"<<std::endl;
                    }
                }
                else // if (v1_connections==2)
                {
                    // v1 connects to a start and an end - must have a loop in the list!
                    osg::notify(osg::NOTICE)<<"v1="<<v1<<" must connect to a start and an end - must have a loop!!!!!."<<std::endl;
                }
            }
            else 
            {
                // v1 and v2 connect to existing lines, now need to fuse them together.
                bool v1_connected_to_start = v1_start_itr != _startPolylineMap.end();
                bool v1_connected_to_end = v1_end_itr != _endPolylineMap.end();
                
                bool v2_connected_to_start = v2_start_itr != _startPolylineMap.end();
                bool v2_connected_to_end = v2_end_itr != _endPolylineMap.end();
                
                if (v1_connected_to_start)
                {
                    if (v2_connected_to_start)
                    {
                        fuse_start_to_start(v1_start_itr, v2_start_itr);
                    }
                    else if (v2_connected_to_end)
                    {
                        fuse_start_to_end(v1_start_itr, v2_end_itr);
                    }
                    else
                    {
                        osg::notify(osg::NOTICE)<<"Error: should not get here!"<<std::endl;
                    }
                }
                else if (v1_connected_to_end)
                {
                    if (v2_connected_to_start)
                    {
                        fuse_start_to_end(v2_start_itr, v1_end_itr);
                    }
                    else if (v2_connected_to_end)
                    {
                        fuse_end_to_end(v1_end_itr, v2_end_itr);
                    }
                    else
                    {
                        osg::notify(osg::NOTICE)<<"Error: should not get here!"<<std::endl;
                    }
                }
                else
                {
                    osg::notify(osg::NOTICE)<<"Error: should not get here!"<<std::endl;
                }
            }
        }
        
        void newline(const osg::Vec3d& v1, const osg::Vec3d& v2)
        {
            RefPolyline* polyline = new RefPolyline;
            polyline->_polyline.push_back(v1);
            polyline->_polyline.push_back(v2);
            _startPolylineMap[v1] = polyline;
            _endPolylineMap[v2] = polyline;
        }
        
        void insertAtStart(const osg::Vec3d& v, PolylineMap::iterator v_start_itr)
        {
            // put v1 at the start of its poyline
            RefPolyline* polyline = v_start_itr->second.get();
            polyline->_polyline.insert(polyline->_polyline.begin(),v);

            // reinsert the polyine at the new v1 end
            _startPolylineMap[v] = polyline;

            // remove the original entry
            _startPolylineMap.erase(v_start_itr);

        }
        
        void insertAtEnd(const osg::Vec3d& v, PolylineMap::iterator v_end_itr)
        {
            // put v1 at the end of its poyline
            RefPolyline* polyline = v_end_itr->second.get();
            polyline->_polyline.push_back(v);

            // reinsert the polyine at the new v1 end
            _endPolylineMap[v] = polyline;

            // remove the original entry
            _endPolylineMap.erase(v_end_itr);

        }

        void fuse_start_to_start(PolylineMap::iterator start1_itr, PolylineMap::iterator start2_itr)
        {
            osg::ref_ptr<RefPolyline> poly1 = start1_itr->second;
            osg::ref_ptr<RefPolyline> poly2 = start2_itr->second;

            PolylineMap::iterator end1_itr = _endPolylineMap.find(poly1->_polyline.back());
            PolylineMap::iterator end2_itr = _endPolylineMap.find(poly2->_polyline.back());

            // clean up the iterators associated with the original polylines
            _startPolylineMap.erase(start1_itr);
            _startPolylineMap.erase(start2_itr);
            _endPolylineMap.erase(end1_itr);
            _endPolylineMap.erase(end2_itr);

            // reverse the first polyline
            poly1->reverse();
            
            // add the second polyline to the first
            poly1->_polyline.insert( poly1->_polyline.end(),
                                     poly2->_polyline.begin(), poly2->_polyline.end() );
            
            _startPolylineMap[poly1->_polyline.front()] = poly1;
            _endPolylineMap[poly1->_polyline.back()] = poly1;

        }

        void fuse_start_to_end(PolylineMap::iterator start_itr, PolylineMap::iterator end_itr)
        {
            osg::ref_ptr<RefPolyline> end_poly = end_itr->second;
            osg::ref_ptr<RefPolyline> start_poly = start_itr->second;
            
            PolylineMap::iterator end_start_poly_itr = _endPolylineMap.find(start_poly->_polyline.back());
            
            // add start_poly to end of end_poly
            end_poly->_polyline.insert( end_poly->_polyline.end(),
                                        start_poly->_polyline.begin(), start_poly->_polyline.end() );
                    
            // reassign the end of the start poly so that it now points to the merged end_poly
            end_start_poly_itr->second = end_poly;

            
            // remove entries for the end of the end_poly and the start of the start_poly
            _endPolylineMap.erase(end_itr);
            _startPolylineMap.erase(start_itr);

            if (end_poly==start_poly)
            {
                _polylines.push_back(end_poly);
            }

        }

        void fuse_end_to_end(PolylineMap::iterator end1_itr, PolylineMap::iterator end2_itr)
        {

            // return;

            osg::ref_ptr<RefPolyline> poly1 = end1_itr->second;
            osg::ref_ptr<RefPolyline> poly2 = end2_itr->second;

            PolylineMap::iterator start1_itr = _startPolylineMap.find(poly1->_polyline.front());
            PolylineMap::iterator start2_itr = _startPolylineMap.find(poly2->_polyline.front());

            // clean up the iterators associated with the original polylines
            _startPolylineMap.erase(start1_itr);
            _startPolylineMap.erase(start2_itr);
            _endPolylineMap.erase(end1_itr);
            _endPolylineMap.erase(end2_itr);

            // reverse the first polyline
            poly2->reverse();
            
            // add the second polyline to the first
            poly1->_polyline.insert( poly1->_polyline.end(),
                                     poly2->_polyline.begin(), poly2->_polyline.end() );
            
            _startPolylineMap[poly1->_polyline.front()] = poly1;
            _endPolylineMap[poly1->_polyline.back()] = poly1;

        }

        void consolidatePolylineLists()
        {
            // move the remaining open ended line segments into the polyline list
            for(PolylineMap::iterator sitr = _startPolylineMap.begin();
                sitr != _startPolylineMap.end();
                ++sitr)
            {
                _polylines.push_back(sitr->second);
            }
        }

        void report()
        {
            osg::notify(osg::NOTICE)<<"report()"<<std::endl;

            osg::notify(osg::NOTICE)<<"start:"<<std::endl;
            for(PolylineMap::iterator sitr = _startPolylineMap.begin();
                sitr != _startPolylineMap.end();
                ++sitr)
            {
                osg::notify(osg::NOTICE)<<"  line - start = "<<sitr->first<<" polyline size = "<<sitr->second->_polyline.size()<<std::endl;
            }

            osg::notify(osg::NOTICE)<<"ends:"<<std::endl;
            for(PolylineMap::iterator eitr = _endPolylineMap.begin();
                eitr != _endPolylineMap.end();
                ++eitr)
            {
                osg::notify(osg::NOTICE)<<"  line - end = "<<eitr->first<<" polyline size = "<<eitr->second->_polyline.size()<<std::endl;
            }

            for(PolylineList::iterator pitr = _polylines.begin();
                pitr != _polylines.end();
                ++pitr)
            {
                osg::notify(osg::NOTICE)<<"polyline:"<<std::endl;
                RefPolyline::Polyline& polyline = (*pitr)->_polyline;
                for(RefPolyline::Polyline::iterator vitr = polyline.begin();
                    vitr != polyline.end();
                    ++vitr)
                {
                    osg::notify(osg::NOTICE)<<"  "<<*vitr<<std::endl;
                }
            }


        }

        void fuse()
        {
             osg::notify(osg::NOTICE)<<"supposed to be doing a fuse..."<<std::endl;
        }

        
    };

    struct TriangleIntersector
    {

        osg::Plane _plane;
        osg::Polytope _polytope;
        bool _hit;

        PolylineConnector _polylineConnector;


        TriangleIntersector()
        {
            _hit = false;
        }

        void set(const osg::Plane& plane, const osg::Polytope& polytope)
        {
            _plane = plane;
            _polytope = polytope;
            _hit = false;
        }

        inline void add(osg::Vec3d& vs, osg::Vec3d& ve)
        {
            if (_polytope.getPlaneList().empty())
            {
                _polylineConnector.add(vs,ve);
            }
            else
            {
                for(osg::Polytope::PlaneList::iterator itr = _polytope.getPlaneList().begin();
                    itr != _polytope.getPlaneList().end();
                    ++itr)
                {
                    osg::Plane& plane = *itr;
                    double ds = plane.distance(vs);
                    double de = plane.distance(ve);
                    
                    if (ds<0.0)
                    {
                        if (de<0.0)
                        {
                            // osg::notify(osg::NOTICE)<<"Disgard segment "<<std::endl;
                            return;
                        }
                        
                        // osg::notify(osg::NOTICE)<<"Trim start vs="<<vs;

                        double div = 1.0/(de-ds);
                        vs = vs*(de*div) - ve*(ds*div);

                        // osg::notify(osg::NOTICE)<<" after vs="<<vs<<std::endl;
                        
                    }
                    else if (de<0.0)
                    {
                        // osg::notify(osg::NOTICE)<<"Trim end ve="<<ve;

                        double div = 1.0/(ds-de);
                        ve = ve*(ds*div) - vs*(de*div);

                        // osg::notify(osg::NOTICE)<<" after ve="<<ve<<std::endl;
                        
                    }
                    
                } 
                // osg::notify(osg::NOTICE)<<"Segment fine"<<std::endl;

                _polylineConnector.add(vs,ve);
                
            }
        }

        inline void operator () (const osg::Vec3& v1,const osg::Vec3& v2,const osg::Vec3& v3, bool)
        {
        
            double d1 = _plane.distance(v1);
            double d2 = _plane.distance(v2);
            double d3 = _plane.distance(v3);
            
            unsigned int numBelow = 0;
            unsigned int numAbove = 0;
            unsigned int numOnPlane = 0;
            if (d1<0) ++numBelow;
            else if (d1>0) ++numAbove;
            else ++numOnPlane;
        
            if (d2<0) ++numBelow;
            else if (d2>0) ++numAbove;
            else ++numOnPlane;
        
            if (d3<0) ++numBelow;
            else if (d3>0) ++numAbove;
            else ++numOnPlane;
            
            // trivially discard triangles that are completely one side of the plane
            if (numAbove==3 || numBelow==3) return;
            
            _hit = true;

            if (numOnPlane==3)
            {
                // triangle lives wholy in the plane
                osg::notify(osg::NOTICE)<<"3"<<std::endl;
                return;
            }

            if (numOnPlane==2)
            {
                // one edge lives wholy in the plane
                osg::notify(osg::NOTICE)<<"2"<<std::endl;
                return;
            }

            if (numOnPlane==1)
            {
                // one point lives wholy in the plane
                osg::notify(osg::NOTICE)<<"1"<<std::endl;
                return;
            }

            osg::Vec3d v[2];
            unsigned int numIntersects = 0;

            if (d1*d2 < 0.0)
            {
                // edge 12 itersects
                double div = 1.0 / (d2-d1);
                v[numIntersects++] = v1* (d2*div) - v2 * (d1*div);
            }

            if (d2*d3 < 0.0)
            {
                // edge 23 itersects
                double div = 1.0 / (d3-d2);
                v[numIntersects++] = v2* (d3*div) - v3 * (d2*div);
            }

            if (d1*d3 < 0.0)
            {
                if (numIntersects<2)
                {
                    // edge 13 itersects
                    double div = 1.0 / (d3-d1);
                    v[numIntersects++] = v1* (d3*div) - v3 * (d1*div);
                }
                else
                {
                    osg::notify(osg::NOTICE)<<"!!! too many intersecting edges found !!!"<<std::endl;
                }
                 
            }

            add(v[0],v[1]);

        }

    };

}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  PlaneIntersector
//
PlaneIntersector::PlaneIntersector(const osg::Plane& plane, const osg::Polytope& boundingPolytope):
    _parent(0),
    _plane(plane),
    _polytope(boundingPolytope)
{
}

PlaneIntersector::PlaneIntersector(CoordinateFrame cf, const osg::Plane& plane, const osg::Polytope& boundingPolytope):
    Intersector(cf),
    _parent(0),
    _plane(plane),
    _polytope(boundingPolytope)
{
}


Intersector* PlaneIntersector::clone(osgUtil::IntersectionVisitor& iv)
{
    if (_coordinateFrame==MODEL && iv.getModelMatrix()==0)
    {
        osg::ref_ptr<PlaneIntersector> pi = new PlaneIntersector(_plane, _polytope);
        pi->_parent = this;
        return pi.release();
    }

    // compute the matrix that takes this Intersector from its CoordinateFrame into the local MODEL coordinate frame
    // that geometry in the scene graph will always be in.
    osg::Matrix matrix;
    switch (_coordinateFrame)
    {
        case(WINDOW): 
            if (iv.getWindowMatrix()) matrix.preMult( *iv.getWindowMatrix() );
            if (iv.getProjectionMatrix()) matrix.preMult( *iv.getProjectionMatrix() );
            if (iv.getViewMatrix()) matrix.preMult( *iv.getViewMatrix() );
            if (iv.getModelMatrix()) matrix.preMult( *iv.getModelMatrix() );
            break;
        case(PROJECTION): 
            if (iv.getProjectionMatrix()) matrix.preMult( *iv.getProjectionMatrix() );
            if (iv.getViewMatrix()) matrix.preMult( *iv.getViewMatrix() );
            if (iv.getModelMatrix()) matrix.preMult( *iv.getModelMatrix() );
            break;
        case(VIEW): 
            if (iv.getViewMatrix()) matrix.preMult( *iv.getViewMatrix() );
            if (iv.getModelMatrix()) matrix.preMult( *iv.getModelMatrix() );
            break;
        case(MODEL):
            if (iv.getModelMatrix()) matrix = *iv.getModelMatrix();
            break;
    }

    osg::Plane plane = _plane;
    plane.transformProvidingInverse(matrix);

    osg::Polytope transformedPolytope;
    transformedPolytope.setAndTransformProvidingInverse(_polytope, matrix);

    osg::ref_ptr<PlaneIntersector> pi = new PlaneIntersector(plane, transformedPolytope);
    pi->_parent = this;
    return pi.release();
}

bool PlaneIntersector::enter(const osg::Node& node)
{
    return !node.isCullingActive() ||
           ( _plane.intersect(node.getBound())==0 && _polytope.contains(node.getBound()) );
}


void PlaneIntersector::leave()
{
    // do nothing.
}


void PlaneIntersector::intersect(osgUtil::IntersectionVisitor& iv, osg::Drawable* drawable)
{
    // osg::notify(osg::NOTICE)<<"PlaneIntersector::intersect(osgUtil::IntersectionVisitor& iv, osg::Drawable* drawable)"<<std::endl;

    if ( _plane.intersect( drawable->getBound() )!=0 ) return;
    if ( !_polytope.contains( drawable->getBound() ) ) return;

    // osg::notify(osg::NOTICE)<<"Succed PlaneIntersector::intersect(osgUtil::IntersectionVisitor& iv, osg::Drawable* drawable)"<<std::endl;

    osg::TriangleFunctor<PlaneIntersectorUtils::TriangleIntersector> ti;
    ti.set(_plane,_polytope);
    drawable->accept(ti);

    ti._polylineConnector.consolidatePolylineLists();

    if (ti._hit)
    {
        Intersections& intersections = getIntersections();
     
        for(PlaneIntersectorUtils::PolylineConnector::PolylineList::iterator pitr = ti._polylineConnector._polylines.begin();
            pitr != ti._polylineConnector._polylines.end();
            ++pitr)
        {

            unsigned int pos = intersections.size();

            intersections.push_back(Intersection());
            Intersection& new_intersection = intersections[pos];

            new_intersection.matrix = iv.getModelMatrix();
            new_intersection.polyline = (*pitr)->_polyline;
            new_intersection.nodePath = iv.getNodePath();
            new_intersection.drawable = drawable;
        }
    }

}


void PlaneIntersector::reset()
{
    Intersector::reset();
    
    _intersections.clear();
}
