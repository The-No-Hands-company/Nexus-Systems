#pragma once
#include <vector>
#include <cmath>
#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>
namespace nexus::utility::geospatial {

class CoordConverter { // WGS84 ↔ UTM, ECEF
public:
    struct Geo { double lat,lon,alt; }; // degrees
    struct Utm { double easting,northing; int zone; char band; };
    struct Ecef { double x,y,z; };

    static Ecef geoToEcef(const Geo& g) {
        double a=6378137, f=1/298.257223563, e2=2*f-f*f;
        double rad=M_PI/180;
        double slat=std::sin(g.lat*rad), clat=std::cos(g.lat*rad);
        double slon=std::sin(g.lon*rad), clon=std::cos(g.lon*rad);
        double N=a/std::sqrt(1-e2*slat*slat);
        return {(N+g.alt)*clat*clon, (N+g.alt)*clat*slon, (N*(1-e2)+g.alt)*slat};
    }

    static Geo ecefToGeo(const Ecef& e) {
        double a=6378137, f=1/298.257223563, e2=2*f-f*f, b=a*(1-f);
        double ep2=(a*a-b*b)/(b*b), p=std::sqrt(e.x*e.x+e.y*e.y);
        double th=std::atan2(e.z*a,p*b);
        double lat=std::atan2(e.z+ep2*b*std::pow(std::sin(th),3),p-e2*a*std::pow(std::cos(th),3));
        double lon=std::atan2(e.y,e.x);
        double slat=std::sin(lat), N=a/std::sqrt(1-e2*slat*slat);
        double alt=p/std::cos(lat)-N;
        return {lat*180/M_PI, lon*180/M_PI, alt};
    }

    static double haversineDist(const Geo& a, const Geo& b) {
        double rad=M_PI/180;
        double dlat=(b.lat-a.lat)*rad, dlon=(b.lon-a.lon)*rad;
        double sa=std::sin(dlat/2), sb=std::sin(dlon/2);
        double h=sa*sa+std::cos(a.lat*rad)*std::cos(b.lat*rad)*sb*sb;
        return 2*6371000*std::atan2(std::sqrt(h),std::sqrt(1-h));
    }
};

class GeohashCodec {
public:
    static std::string encode(double lat, double lon, int precision=12) {
        static const char* b32="0123456789bcdefghjkmnpqrstuvwxyz";
        double lat_lo=-90,lat_hi=90, lon_lo=-180,lon_hi=180;
        std::string hash; bool even=true; int bit=0, chr=0;
        while(hash.size()<size_t(precision)) {
            if(even) { double mid=(lon_lo+lon_hi)/2;
                if(lon>mid){chr=(chr<<1)|1;lon_lo=mid;}else{chr<<=1;lon_hi=mid;} }
            else { double mid=(lat_lo+lat_hi)/2;
                if(lat>mid){chr=(chr<<1)|1;lat_lo=mid;}else{chr<<=1;lat_hi=mid;} }
            even=!even;
            if(++bit==5){hash+=b32[chr&0x1F];bit=0;chr=0;}
        }
        return hash;
    }
    static double decodeDistance(const std::string& a, const std::string& b) {
        int diff=0;
        for(size_t i=0;i<std::min(a.size(),b.size());++i) if(a[i]!=b[i]){diff=i;break;}
        return 5000000*std::pow(0.5,diff);
    }
};

class GeofenceChecker {
public:
    struct Polygon { std::vector<double> lats, lons; };
    static bool pointInPolygon(double lat, double lon, const Polygon& poly) {
        bool inside=false;
        for(size_t i=0,j=poly.lats.size()-1; i<poly.lats.size(); j=i++)
            if(((poly.lats[i]>lat)!=(poly.lats[j]>lat))&&
               (lon<(poly.lons[j]-poly.lons[i])*(lat-poly.lats[i])/(poly.lats[j]-poly.lats[i])+poly.lons[i]))
                inside=!inside;
        return inside;
    }
};

} // namespace nexus::utility::geospatial
