#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

constexpr double pi=3.14159265358979323846;
constexpr double latitude=49*pi/180;
constexpr double radius=1.4;
constexpr double center_z=2.88;
constexpr double depth=.72;
constexpr int samples=37;
using Rgb=std::array<double,3>;
using Direction=std::array<double,3>;
const std::array<double,3> declinations{{23.44,0,-23.44}};
const std::array<double,3> pane_depth{{0,.36,.72}};
const std::array<double,3> motif_x{{-.70,0,.70}};
const std::array<const char*,3> headline{{"Summer solstice","Equinox","Winter solstice"}};
const std::array<const char*,3> symbols{{"Sunburst","Three-bar cross","Four-point star"}};
const std::array<Rgb,3> accent{{{1,.67,.31},{1,.91,.63},{.47,.75,1}}};

struct Point { double x,y; };
struct Ray { double x_over_z,y_over_z,north,up; };
struct Glass { std::array<std::uint8_t,3> rgb; int motif; };
struct Light { Rgb rgb{}; double aperture=0; std::array<double,3> registered{}; };

Direction sun_vector(double declination,double hour) {
    double d=declination*pi/180, h=(hour-12)*pi/12;
    return {-std::cos(d)*std::sin(h),
            std::cos(latitude)*std::sin(d)-std::sin(latitude)*std::cos(d)*std::cos(h),
            std::sin(latitude)*std::sin(d)+std::cos(latitude)*std::cos(d)*std::cos(h)};
}

const std::array<double,3> aimed=[]{
    std::array<double,3> values;
    for(int i=0;i<3;++i) {
        auto s=sun_vector(declinations[i],12);
        values[i]=-s[2]/s[1];
    }
    return values;
}();

std::vector<Ray> sun_disk(Direction sun,int count) {
    const double horizontal=std::hypot(sun[0],sun[1]);
    const Direction east{-sun[1]/horizontal,sun[0]/horizontal,0};
    const Direction high{-sun[2]*sun[0]/horizontal,-sun[2]*sun[1]/horizontal,horizontal};
    std::vector<Ray> rays;
    for(int i=0;i<count;++i) {
        const double theta=i*pi*(3-std::sqrt(5.0));
        const double r=std::tan(.27*pi/180)*std::sqrt(i/(count-.5));
        const double a=r*std::cos(theta),b=r*std::sin(theta);
        Direction direction;
        for(int coordinate=0;coordinate<3;++coordinate)
            direction[coordinate]=sun[coordinate]+a*east[coordinate]+b*high[coordinate];
        if(direction[1]<0 && direction[2]>.00001)
            rays.push_back({direction[0]/direction[2],direction[1]/direction[2],direction[1],direction[2]});
    }
    return rays;
}

double radial_edge(double theta,int points,double wide,double inner) {
    const double step=pi/points;
    const double place=(theta+2*pi)/step;
    const int index=static_cast<int>(std::floor(place));
    const double phi=(place-index)*step;
    const double a=index%2==0?wide:inner, b=index%2==0?inner:wide;
    return a*b*std::sin(step)/(b*std::sin(step-phi)+a*std::sin(phi));
}

bool shape(int season,double x,double v) {
    if(season==0) {
        if(std::abs(x)>.20 || std::abs(v)>.172) return false;
        const double u=x/.20,w=v/.172,r=std::hypot(u,w);
        return r>.27 && r<=radial_edge(std::atan2(u,w),8,1,.72);
    }
    if(season==1) {
        const double u=std::abs(x);
        return (u<=.036 && std::abs(v)<=.214)
            || (u<=.099 && std::abs(v-.132)<=.021)
            || (u<=.204 && std::abs(v-.052)<=.028)
            || (u<=.122 && std::abs(v+.081+.43*x)<=.023);
    }
    if(std::abs(x)>.282 || std::abs(v)>.449) return false;
    const double u=x/.282,w=v/.449;
    return std::hypot(u,w)<=radial_edge(std::atan2(u,w),4,1,.42);
}

int motif_at(int pane,double x,double z) {
    const double d=pane_depth[pane]-depth/2;
    for(int season=0;season<3;++season) {
        const double sample_x=x-motif_x[season];
        const double sample_y=-(z+d*aimed[season])/aimed[season];
        if(shape(season,sample_x,sample_y)) return season;
    }
    return -1;
}

const std::array<std::array<Rgb,3>,3> motif_tint{{
    {{{.99,.88,.43},{.99,.82,.64},{.98,.86,.59}}},
    {{{.99,.98,.83},{.97,.96,.89},{.98,.94,.84}}},
    {{{.65,.86,.99},{.75,.90,.99},{.72,.82,.98}}}
}};

const std::array<Rgb,11> palette{{
    {.68,.24,.51}, {.69,.33,.74}, {.35,.40,.84}, {.25,.61,.88},
    {.31,.79,.74}, {.34,.72,.47}, {.78,.74,.38}, {.90,.59,.25},
    {.86,.36,.26}, {.85,.43,.58}, {.70,.74,.77}
}};

struct Atom {
    Point center;
    int color;
};

double hash(int i) {
    auto value=static_cast<std::uint32_t>(i)*747796405u+2891336453u;
    auto word=((value>>((value>>28)+4))^value)*277803737u;
    return ((word>>22)^word)/4294967296.0;
}

using Mosaic=std::vector<Atom>;
Mosaic mosaic(int pane) {
    Mosaic cells;
    int index=1+pane*174;
    const std::array<int,5> divisions{{7,11,16,21,23}};
    for(size_t ring=0;ring<divisions.size();++ring){
        for(int i=0;i<divisions[ring];++i,++index){
            double a=2*pi*(i+(.105+.50*hash(index+7)))/divisions[ring]+.14*pane+.26*ring;
            double r=.24+.272*ring+.13*(hash(index+81)-.5);
            int swatch=static_cast<int>(hash(index+25)*11);
            cells.push_back({{r*std::cos(a),r*std::sin(a)},swatch});
        }
    }
    cells.push_back({{0,0},(3*pane+1)%11});
    return cells;
}

Glass glass_at(int pane,const Mosaic& cells,double x,double z) {
    const int motif=motif_at(pane,x,z);
    if(motif>=0) {
        const auto& color=motif_tint[motif][pane];
        return {{{static_cast<std::uint8_t>(255*color[0]),static_cast<std::uint8_t>(255*color[1]),
                  static_cast<std::uint8_t>(255*color[2])}},motif};
    }
    double first=100,second=100;
    int best=0;
    for(size_t i=0;i<cells.size();++i) {
        auto c=cells[i].center;
        double distance=(x-c.x)*(x-c.x)+(z-c.y)*(z-c.y);
        if(distance<first) {second=first;first=distance;best=i;}
        else if(distance<second) second=distance;
    }
    const double bevel=std::clamp((second-first)*520,.0,1.0);
    const double variation=.91+.056*std::sin(39*z+7*x)+.032*std::sin(123*x+39*z);
    const auto color=palette[cells[best].color];
    Rgb mix;
    for(int k=0;k<3;++k) mix[k]=.044+(.19+.81*bevel)*(.61*color[k]*variation);
    return {{{static_cast<std::uint8_t>(255*mix[0]),static_cast<std::uint8_t>(255*mix[1]),
              static_cast<std::uint8_t>(255*mix[2])}},-1};
}

struct Textures {
    static constexpr int resolution=900;
    std::array<std::vector<Glass>,3> panes;
    Textures(){
        for(int p=0;p<3;++p) {
            const auto cells=mosaic(p);
            panes[p].resize(resolution*resolution);
            for(int y=0;y<resolution;++y){
                for(int x=0;x<resolution;++x){
                    const double u=(x+.5)*2*radius/resolution-radius;
                    const double v=(y+.5)*2*radius/resolution-radius;
                    if(u*u+v*v<=radius*radius)
                        panes[p][y*resolution+x]=glass_at(p,cells,u,v);
                }
            }
        }
    }

    const Glass& sample(int pane,double x,double z) const {
        const int i=std::clamp(static_cast<int>((x+radius)*resolution/(2*radius)),0,resolution-1);
        const int j=std::clamp(static_cast<int>((z+radius)*resolution/(2*radius)),0,resolution-1);
        return panes[pane][j*resolution+i];
    }
};

struct FloorRay {
    double x_over_z, y_over_z, z0, dz, front;
};

std::vector<FloorRay> floor_rays(const Direction& sun,int count,double spacing=.36){
    std::vector<FloorRay> rays;
    for(const auto& ray:sun_disk(sun,count)) {
        double z0=-1/ray.y_over_z;
        rays.push_back({ray.x_over_z,ray.y_over_z,z0,-spacing*z0,depth/2-spacing});
    }
    return rays;
}

Light project(const Textures& archive,const std::vector<FloorRay>& rays,int count,double x,double y) {
    Light light;
    if(y<depth) return light;
    constexpr double inverse_cube=1./(255.0*255*255);
    for(const auto& ray:rays) {
        const double z_first=(y-ray.front)*ray.z0;
        const double shift=ray.x_over_z*ray.dz;
        const double x_first=x+ray.x_over_z*z_first;
        const double z0=z_first-center_z;
        const double z2=z0+2*ray.dz,x2=x_first+2*shift;
        if(x_first*x_first+z0*z0>=radius*radius || x2*x2+z2*z2>=radius*radius) continue;
        // The circle is convex, so a ray within both end apertures is within the middle aperture.
        const Glass& a=archive.sample(0,x_first,z0);
        const Glass& b=archive.sample(1,x_first+shift,z0+ray.dz);
        const Glass& c=archive.sample(2,x2,z2);
        for(int channel=0;channel<3;++channel)
            light.rgb[channel]+=a.rgb[channel]*b.rgb[channel]*c.rgb[channel];
        light.aperture+=1;
        if(a.motif>=0 && a.motif==b.motif && b.motif==c.motif) ++light.registered[a.motif];
    }
    for(double& channel:light.rgb) channel*=inverse_cube/count;
    for(double& energy:light.registered) energy/=count;
    light.aperture/=count;
    return light;
}

double noon_landing(int season) { return depth/2+center_z/aimed[season]; }

struct PatchBounds {
    double min_x=100,max_x=-100,min_y=100,max_y=-100;
    bool present=false;
};

PatchBounds patch_bounds(Direction sun) {
    PatchBounds view;
    const auto rays=floor_rays(sun,1);
    if(rays.empty()) return view;
    const auto ray=rays[0];
    // Footprint intersection. A y-slice of each projected disk is an x interval.
    const double lo=std::max(depth,depth+(center_z-radius)/ray.z0);
    const double hi=std::min(16.0,(center_z+radius)/ray.z0);
    for (double y=lo;y<hi;y+=(hi-lo)/75) {
        double low=-8.5,high=8.5;
        for(double d:pane_depth) {
            double z=(y-d)*ray.z0;
            if(std::abs(z-center_z)>radius){high=low-1;break;}
            double r=std::sqrt(radius*radius-std::pow(z-center_z,2));
            double x=-ray.x_over_z*z;
            low=std::max(low,x-r);
            high=std::min(high,x+r);
        }
        if(low>=high) continue;
        view.present=true;
        view.min_x=std::min(view.min_x,low);
        view.max_x=std::max(view.max_x,high);
        view.min_y=std::min(view.min_y,y);
        view.max_y=std::max(view.max_y,y);
    }
    return view;
}

double luminance(Rgb rgb) {return .2126*rgb[0]+.7152*rgb[1]+.0722*rgb[2];}

int encode(double linear){
    static const auto table=[] {
        std::array<int,4097> bytes;
        for(size_t i=0;i<bytes.size();++i) {
            double capped=1-std::exp(-static_cast<double>(i)/1024.0);
            double srgb=capped<=.00313?12.92*capped:1.055*std::pow(capped,1/2.4)-.055;
            bytes[i]=static_cast<int>(std::round(255*srgb));
        }
        return bytes;
    }();
    return table[std::clamp(static_cast<int>(1024*linear+.5),0,4096)];
}

double grout(double position){
    return std::max(0.0,1-std::abs(std::remainder(position,.32))/.006);
}

std::uint32_t stone(Rgb illumination,double x,double y,double day) {
    const double tile=std::sin(std::floor((x+.14)/.32)*5.3+std::floor((y+.17)/.32)*3.7);
    const unsigned bits=static_cast<unsigned>(static_cast<int>((x+32)*1193)*524287ll+
                                            static_cast<int>((y+32)*1681)*19661ll);
    const double grain=(((bits*1664525u+1013904223u)>>12)&255)/255.0-.5;
    const double joint=std::max(grout(x+.14),grout(y+.17));
    const double material=.88+.071*tile+.073*grain-.35*joint;
    const double ambient=.012+.037*day;
    int red=encode(material*(ambient*.84+illumination[0]*2.55));
    int green=encode(material*(ambient*.95+illumination[1]*2.38));
    int blue=encode(material*(ambient*1.13+illumination[2]*2.30));
    return (red<<16)|(green<<8)|blue;
}
