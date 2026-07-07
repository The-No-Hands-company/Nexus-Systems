#pragma once

#include <vector>
#include <cmath>
#include <string>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <limits>

namespace nexus::utility::vision {

class ContourIntegrityChecker {
public:
    struct Contour { std::vector<double> x, y; bool closed = false; };

    struct ContourAnalysis {
        size_t contour_count = 0;
        size_t open_contours = 0;
        size_t self_intersecting = 0;
        size_t nested_contours = 0;
        double avg_perimeter = 0, avg_area = 0;
        double circularity_avg = 0;
        double convexity_avg = 0;
    };

    static ContourAnalysis analyze(const std::vector<Contour>& contours) {
        ContourAnalysis r;
        r.contour_count = contours.size();
        double perim_sum = 0, area_sum = 0, circ_sum = 0, conv_sum = 0;
        int valid = 0;

        for (auto& c : contours) {
            if (!c.closed) r.open_contours++;
            if (c.x.size() < 3) continue;

            if (isSelfIntersecting(c)) r.self_intersecting++;

            double peri = perimeter(c);
            double area = polygonArea(c);
            perim_sum += peri; area_sum += area;

            double circ = (peri > 0) ? 4*M_PI*area/(peri*peri) : 0;
            circ_sum += circ;

            double hull_area = convexHullArea(c);
            conv_sum += (hull_area > 0) ? area/hull_area : 0;
            valid++;
        }

        if (valid > 0) {
            r.avg_perimeter = perim_sum / valid;
            r.avg_area = area_sum / valid;
            r.circularity_avg = circ_sum / valid;
            r.convexity_avg = conv_sum / valid;
        }

        // Nested contour detection
        for (size_t i = 0; i < contours.size(); ++i)
            for (size_t j = i+1; j < contours.size(); ++j)
                if (pointInContour(contours[i].x[0], contours[i].y[0], contours[j]))
                    { r.nested_contours++; break; }

        return r;
    }

    static std::string report(const ContourAnalysis& r) {
        std::ostringstream oss; oss << std::fixed << std::setprecision(3);
        oss << "═══ Contour Integrity ═══\n";
        oss << "  Contours:       " << r.contour_count << "\n";
        oss << "  Open:           " << r.open_contours << (r.open_contours > 0 ? " ⚠" : " ✓") << "\n";
        oss << "  Self-intersect: " << r.self_intersecting << (r.self_intersecting > 0 ? " ⚠" : " ✓") << "\n";
        oss << "  Nested:         " << r.nested_contours << "\n";
        oss << "  Avg Perimeter:  " << r.avg_perimeter << "\n";
        oss << "  Avg Area:       " << r.avg_area << "\n";
        oss << "  Circularity:    " << r.circularity_avg << "\n";
        oss << "  Convexity:      " << r.convexity_avg << "\n";
        return oss.str();
    }

private:
    static double perimeter(const Contour& c) {
        double p = 0;
        for (size_t i = 1; i < c.x.size(); ++i)
            p += std::hypot(c.x[i]-c.x[i-1], c.y[i]-c.y[i-1]);
        if (c.closed && c.x.size()>1)
            p += std::hypot(c.x.back()-c.x[0], c.y.back()-c.y[0]);
        return p;
    }

    static double polygonArea(const Contour& c) {
        double a = 0; auto n = c.x.size();
        for (size_t i = 0; i < n; ++i)
            a += c.x[i]*c.y[(i+1)%n] - c.x[(i+1)%n]*c.y[i];
        return std::abs(a)/2;
    }

    static bool isSelfIntersecting(const Contour& c) {
        auto n = c.x.size();
        for (size_t i = 0; i < n; ++i) {
            for (size_t j = i+2; j < n; ++j) {
                if (i==0 && j==n-1) continue;
                if (segmentsIntersect(c.x[i],c.y[i],c.x[(i+1)%n],c.y[(i+1)%n],
                                       c.x[j],c.y[j],c.x[(j+1)%n],c.y[(j+1)%n]))
                    return true;
            }
        }
        return false;
    }

    static bool segmentsIntersect(double x1,double y1,double x2,double y2,
                                   double x3,double y3,double x4,double y4) {
        auto orient = [](double ax,double ay,double bx,double by,double cx,double cy) {
            return (bx-ax)*(cy-ay) - (by-ay)*(cx-ax);
        };
        double o1=orient(x1,y1,x2,y2,x3,y3), o2=orient(x1,y1,x2,y2,x4,y4);
        double o3=orient(x3,y3,x4,y4,x1,y1), o4=orient(x3,y3,x4,y4,x2,y2);
        return (o1*o2<0) && (o3*o4<0);
    }

    static double convexHullArea(const Contour& c) {
        // Simplified: use bounding box area as approximation
        double minx=std::numeric_limits<double>::max(), maxx=std::numeric_limits<double>::lowest();
        double miny=minx, maxy=maxx;
        for (size_t i=0; i<c.x.size(); ++i) {
            minx=std::min(minx,c.x[i]); maxx=std::max(maxx,c.x[i]);
            miny=std::min(miny,c.y[i]); maxy=std::max(maxy,c.y[i]);
        }
        return (maxx-minx)*(maxy-miny);
    }

    static bool pointInContour(double px, double py, const Contour& c) {
        bool inside = false;
        auto n = c.x.size();
        for (size_t i=0, j=n-1; i<n; j=i++)
            if (((c.y[i]>py) != (c.y[j]>py)) &&
                (px < (c.x[j]-c.x[i])*(py-c.y[i])/(c.y[j]-c.y[i])+c.x[i]))
                inside = !inside;
        return inside;
    }
};

} // namespace nexus::utility::vision
