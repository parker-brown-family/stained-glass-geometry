#include "painted_glass.hpp"
#include <cairo.h>
#include <pango/pangocairo.h>

constexpr int width=1920,height=1080,count=360;
constexpr Rgb sun_ink{1,.82,.60};
constexpr Rgb offwhite{.96,.92,.83};
constexpr Rgb quiet{.66,.72,.75};
constexpr Rgb seam{.57,.60,.63};
#include "scene-typeset.inc"

void polygon(cairo_t* cr,const std::vector<Point>& points,Rgb color,double opacity=1){
    source(cr,color,opacity);
    cairo_move_to(cr,points[0].x,points[0].y);
    for(size_t i=1;i<points.size();++i) cairo_line_to(cr,points[i].x,points[i].y);
    cairo_close_path(cr);
    cairo_fill(cr);
}

double altitude(Direction vector){return std::asin(vector[2])*180/pi;}
double azimuth(Direction vector){
    double bearing=std::atan2(vector[0],vector[1])*180/pi;
    return bearing<0?bearing+360:bearing;
}
double sunrise(double declination){
    return 12-std::acos(-std::tan(latitude)*std::tan(declination*pi/180))*12/pi;
}

double clock_hour(double t,double dawn){
    static const std::array<double,9> seconds{{0,1,2.8,4.1,6,7.9,9.2,11,12}};
    const std::array<double,9> hours{{0,dawn,10.4,11.45,12,12.55,13.60,24-dawn,24}};
    if(t>=12) return 24;
    std::array<double,8> slopes;
    for(size_t i=0;i<slopes.size();++i)
        slopes[i]=(hours[i+1]-hours[i])/(seconds[i+1]-seconds[i]);
    std::array<double,9> tangent;
    tangent[0]=slopes[0];tangent.back()=slopes.back();
    for(size_t i=1;i+1<tangent.size();++i) tangent[i]=2/(1/slopes[i-1]+1/slopes[i]);
    size_t i=std::upper_bound(seconds.begin(),seconds.end(),t)-seconds.begin()-1;
    double span=seconds[i+1]-seconds[i],a=(t-seconds[i])/span,b=a*a,c=b*a;
    return (2*c-3*b+1)*hours[i]+(c-2*b+a)*span*tangent[i]
           +(-2*c+3*b)*hours[i+1]+(c-b)*span*tangent[i+1];
}

using Surface=cairo_surface_t;
struct Domain { double x0,x1,y0,y1; };
struct Photos {
    Surface* room=cairo_image_surface_create(CAIRO_FORMAT_RGB24,458,400);
    Surface* close=cairo_image_surface_create(CAIRO_FORMAT_RGB24,535,494);
    std::array<Surface*,3> panes{};
    ~Photos(){
        cairo_surface_destroy(room);
        cairo_surface_destroy(close);
        for(auto* pane:panes) if(pane) cairo_surface_destroy(pane);
    }
};
struct Result { std::array<double,3> register_area{}; double aperture_area=0; };

void pane_photos(Photos& photos,const Textures& glass){
    const int resolution=Textures::resolution;
    for(int p=0;p<3;++p){
        auto* surface=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,resolution,resolution);
        photos.panes[p]=surface;
        int stride=cairo_image_surface_get_stride(surface);
        unsigned char* data=cairo_image_surface_get_data(surface);
        for(int row=0;row<resolution;++row) {
            auto* out=reinterpret_cast<std::uint32_t*>(data+row*stride);
            for(int col=0;col<resolution;++col){
                auto& pixel=glass.panes[p][(resolution-row-1)*resolution+col];
                if(pixel.rgb[0]==0) {out[col]=0;continue;}
                const int r=encode(pixel.rgb[0]/255.*2),g=encode(pixel.rgb[1]/255.*2),b=encode(pixel.rgb[2]/255.*2);
                out[col]=0xff000000|(r<<16)|(g<<8)|b;
            }
        }
        cairo_surface_mark_dirty(surface);
    }
}

Result floor(Surface* surface,const Textures& glass,const std::vector<FloorRay>& rays,
             PatchBounds bounds,Domain domain,double sunlight,double ambient,int quality){
    cairo_surface_flush(surface);
    auto* data=cairo_image_surface_get_data(surface);
    const int stride=cairo_image_surface_get_stride(surface);
    const int w=cairo_image_surface_get_width(surface),h=cairo_image_surface_get_height(surface);
    const double pixel_area=(domain.x1-domain.x0)*(domain.y1-domain.y0)/(w*h);
    Result result;
    for(int row=0;row<h;++row){
        auto* out=reinterpret_cast<std::uint32_t*>(data+row*stride);
        double y=std::lerp(domain.y0,domain.y1,(row+.5)/h);
        for(int col=0;col<w;++col){
            double x=std::lerp(domain.x0,domain.x1,(col+.5)/w);
            Light light;
            if(bounds.present && y>=bounds.min_y-.65 && y<=bounds.max_y+.65 &&
               x>=bounds.min_x-.65 && x<=bounds.max_x+.65){
                light=project(glass,rays,quality,x,y);
                result.aperture_area+=light.aperture;
                for(int i=0;i<3;++i) result.register_area[i]+=light.registered[i];
            }
            Rgb energy;
            for(int k=0;k<3;++k) energy[k]=light.rgb[k]*sunlight;
            out[col]=stone(energy,x,y,ambient);
        }
    }
    result.aperture_area*=pixel_area;
    for(double& each:result.register_area) each*=pixel_area;
    cairo_surface_mark_dirty(surface);
    return result;
}

Point world(double x,double y,double z=0) {return {546+51*x+11.7*y,622+6*x+18*y-76*z};}
constexpr Domain hall{-8.5,8.5,0,16};

void warp(cairo_t* cr,Surface* image,Point top_left,Point top_right,Point bottom_left,double opacity=1){
    const double w=cairo_image_surface_get_width(image),h=cairo_image_surface_get_height(image);
    cairo_matrix_t matrix;
    cairo_matrix_init(&matrix,(top_right.x-top_left.x)/w,(top_right.y-top_left.y)/w,
                      (bottom_left.x-top_left.x)/h,(bottom_left.y-top_left.y)/h,top_left.x,top_left.y);
    cairo_save(cr);
    cairo_transform(cr,&matrix);
    cairo_set_source_surface(cr,image,0,0);
    cairo_pattern_set_filter(cairo_get_source(cr),CAIRO_FILTER_BILINEAR);
    cairo_paint_with_alpha(cr,opacity);
    cairo_restore(cr);
}

void pane_outline(cairo_t* cr,double y,Rgb color,double thickness,double alpha=1) {
    const Point a=world(radius,y,center_z);
    cairo_move_to(cr,a.x,a.y);
    for(int i=1;i<145;++i) {
        const double t=i*2*pi/144;
        auto at=world(radius*std::cos(t),y,center_z+radius*std::sin(t));
        cairo_line_to(cr,at.x,at.y);
    }
    source(cr,color,alpha);
    cairo_set_line_width(cr,thickness);
    cairo_stroke(cr);
}

void stonewall(cairo_t* cr,double day) {
    const auto wall_box=[&](double x0,double x1,double z0,double z1,Rgb color){
        polygon(cr,{world(x0,0,z0),world(x1,0,z0),world(x1,0,z1),world(x0,0,z1)},color);
    };
    const double tint=.1+.06*day;
    wall_box(-8.5,8.5,0,4.62,{tint,.126+.056*day,.163+.047*day});
    for(int row=0;row<13;++row) {
        double z0=.37*row,z1=std::min(4.62,z0+.357);
        for(int col=0;col<16;++col) {
            double x0=-8.5+(col-.5*(row%2))*1.16,x1=x0+1.141;
            if(x1<-8.5 || x0>8.5) continue;
            double spot=.028*hash(row*99+col);
            wall_box(std::max(-8.5,x0),std::min(8.5,x1),z0,z1,
                     {tint-.024+spot,.098+.052*day+spot,.129+.046*day+spot});
        }
    }
    for(double x:{-7.45,-4.45,4.45,7.45}) {
        wall_box(x-.18,x+.18,0,4.62,{.13+.06*day,.16+.048*day,.20+.038*day});
        wall_box(x-.18,x-.10,0,4.62,{.16+.06*day,.19+.048*day,.22+.038*day});
    }
    wall_box(-8.5,8.5,0,.136,{.16+.062*day,.20+.047*day,.24+.035*day});
}

void sun_glyph(cairo_t* cr,Point at,Rgb color) {
    halo(cr,at,39,color,.46);
    orb(cr,at,10,color);
    orb(cr,at,6,offwhite);
}

void overview(cairo_t* cr,const Photos& photos,const Direction& dir,PatchBounds bounds,double day){
    const double stage_x=70,stage_y=208,stage_w=1031,stage_h=693;
    micro(cr,{stage_x,170},"WINDOW & FLOOR");
    const double alt=altitude(dir);
    std::string sun_label=alt>0 ? std::to_string(int(std::round(alt)))+"°  sun elevation" : "Sun below the horizon";
    text(cr,{stage_x+stage_w,168},sun_label,23,quiet,"Adwaita Sans",500,1);
    cairo_save(cr);
    rounded(cr,stage_x,stage_y,stage_w,stage_h,23);
    cairo_clip(cr);
    auto* g=cairo_pattern_create_linear(0,stage_y,0,stage_y+stage_h);
    cairo_pattern_add_color_stop_rgb(g,0,.052+.090*day,.087+.10*day,.126+.115*day);
    cairo_pattern_add_color_stop_rgb(g,.64,.050,.076,.106);
    cairo_pattern_add_color_stop_rgb(g,1,.027,.041,.062);
    cairo_set_source(cr,g);
    cairo_paint(cr);
    cairo_pattern_destroy(g);
    stonewall(cr,day);
    if(alt>=0){
        double bearing=azimuth(dir);
        Point at{stage_x+80+std::clamp((bearing-35)/290.0,0.0,1.0)*870,282-.79*alt};
        halo(cr,at,158,sun_ink,.13);
        sun_glyph(cr,at,sun_ink);
    }
    warp(cr,photos.room,world(hall.x0,hall.y0),world(hall.x1,hall.y0),world(hall.x0,hall.y1));
    for(int p=0;p<3;++p){
        double y=pane_depth[p];
        pane_outline(cr,y,{.18,.14,.09},14,.99);
        pane_outline(cr,y,{.55,.40,.22},8,.95);
        pane_outline(cr,y,{.14,.09,.05},4,.95);
        if(p==2) {
            warp(cr,photos.panes[p],world(-radius,y,center_z+radius),
                 world(radius,y,center_z+radius),world(-radius,y,center_z-radius),.43+.55*day);
            pane_outline(cr,y,{.78,.62,.36},1.7,.85);
        }
    }
    if(bounds.present) {
        double px=(bounds.min_x+bounds.max_x)/2, py=(bounds.min_y+bounds.max_y)/2;
        const auto rayset=floor_rays(dir,1);
        if(!rayset.empty()){
            const auto ray=rayset[0];
            const double z=py*ray.z0+ray.dz;
            Point a=world(px+ray.x_over_z*z,pane_depth[1],z);
            Point b=world(px,py);
            const std::array<double,2> dashes{6,6};
            cairo_set_dash(cr,dashes.data(),2,0);
            stroke(cr,a,b,{1,.88,.66},1.9,.55);
            cairo_set_dash(cr,nullptr,0,0);
        }
    }
    cairo_restore(cr);
    rounded(cr,stage_x,stage_y,stage_w,stage_h,23);
    source(cr,seam,.51);
    cairo_set_line_width(cr,1.2);
    cairo_stroke(cr);
}

void detail(cairo_t* cr,Surface* texture,bool receiving){
    const double x=1137,y=208,w=713,h=693;
    micro(cr,{x,170},"FLOOR DETAIL");
    text(cr,{1850,168},"Window side ↑",23,quiet,"Adwaita Sans",500,1);
    cairo_save(cr);
    rounded(cr,x,y,w,h,23);
    cairo_clip(cr);
    warp(cr,texture,{x,y},{x+w,y},{x,y+h});
    if(!receiving){
        text(cr,{x+w*.5,y+h*.5-17},"No direct light on this floor",29,quiet,
             "Adwaita Sans",500,.5);
    }
    cairo_restore(cr);
    rounded(cr,x,y,w,h,23);
    source(cr,seam,.51);
    cairo_set_line_width(cr,1.2);
    cairo_stroke(cr);
}

void clock_rail(cairo_t* cr,int season,double hour,double dawn) {
    const double x=72,end=1850,y=988;
    auto at=[&](double t){return x+(end-x)*t/24;};
    stroke(cr,{x,y},{end,y},quiet,1.5,.49);
    stroke(cr,{at(dawn),y},{at(24-dawn),y},accent[season],3,.55);
    for(int time:{0,6,12,18,24}){
        stroke(cr,{at(time),y-6},{at(time),y+6},quiet,1.5,.69);
        char t[9];std::snprintf(t,sizeof(t),"%02d:00",time);
        text(cr,{at(time),1007},t,19,quiet,"Adwaita Mono",500,time==0?0:time==24?1:.5);
    }
    halo(cr,{at(hour),y},18,accent[season],.5);
    orb(cr,{at(hour),y},6,offwhite);
}

void composition(cairo_t* cr,Photos& photos,const Textures& glass,int season,int index){
    const double dawn=sunrise(declinations[season]);
    const double hour=clock_hour(12.*index/(count-1),dawn);
    Direction sun=sun_vector(declinations[season],hour);
    const double above=std::max(0.,sun[2]);
    const double ambient=std::clamp((altitude(sun)+5)/33.,0.,1.);
    const double solar=above<=0?0:std::pow(above,.50);
    const auto bounds=patch_bounds(sun);
    const auto broad_rays=floor_rays(sun,9);
    const auto deep_rays=floor_rays(sun,samples);
    const auto wide_result=floor(photos.room,glass,broad_rays,bounds,hall,solar,ambient,9);
    const double zoom=std::exp(-std::pow((hour-12)/.83,4));
    const double span=bounds.present?std::clamp(std::max(bounds.max_x-bounds.min_x,bounds.max_y-bounds.min_y)*1.18,2.2,12.):4.3;
    const double crop=std::lerp(span,1.65,zoom);
    double aim_x=motif_x[season],aim_y=noon_landing(season);
    if(bounds.present&&!deep_rays.empty()){
        aim_x-=center_z*deep_rays[0].x_over_z;
        aim_y=depth/2-center_z*deep_rays[0].y_over_z;
        aim_x=std::lerp((bounds.min_x+bounds.max_x)/2,aim_x,zoom);
        aim_y=std::lerp((bounds.min_y+bounds.max_y)/2,aim_y,zoom);
    }
    const Domain focus{aim_x-crop/2,aim_x+crop/2,aim_y-crop*.463,aim_y+crop*.463};
    const auto zoom_result=floor(photos.close,glass,deep_rays,bounds,focus,solar,ambient,samples);

    auto* wash=cairo_pattern_create_linear(0,0,width,height);
    cairo_pattern_add_color_stop_rgb(wash,0,.063+.064*ambient,.072+.027*ambient,.087+.009*ambient);
    cairo_pattern_add_color_stop_rgb(wash,.59,.044,.059,.076);
    cairo_pattern_add_color_stop_rgb(wash,1,.033,.045,.063);
    cairo_set_source(cr,wash);cairo_paint(cr);cairo_pattern_destroy(wash);
    halo(cr,{122,85},590,accent[season],.060+.061*ambient);
    micro(cr,{73,24},"THREE FIXED PANES",accent[season]);
    text(cr,{69,45},headline[season],74,offwhite,"Nimbus Roman",400);
    if(season==1) micro(cr,{549,83},"MARCH & SEPTEMBER");
    micro(cr,{1850,27},"LOCAL SOLAR TIME",quiet,1);
    const int mins=std::clamp(static_cast<int>(std::floor(hour*60+1e-8)),0,1440);
    char clock[16];std::snprintf(clock,sizeof(clock),"%02d:%02d",mins/60,mins%60);
    text(cr,{1853,46},clock,70,offwhite,"Adwaita Mono",500,1);
    stroke(cr,{70,149},{1852,149},quiet,1,.35);
    overview(cr,photos,sun,bounds,ambient);
    detail(cr,photos.close,zoom_result.aperture_area>.012);
    const bool aligned=zoom_result.register_area[season]>.012 && std::abs(hour-12)<.21;
    const char* status=aligned?symbols[season]:wide_result.aperture_area>.025?"Overlapping colours":"";
    text(cr,{1137,912},status,30,aligned?accent[season]:quiet,"Adwaita Sans",570);
    clock_rail(cr,season,hour,dawn);

    if(index%30==0||index==count-1||index==179){
        std::fprintf(stderr,"%d %.5f %.3f  %.4f  %.4f %.4f %.4f  %.4f\n",index,hour,altitude(sun),
          zoom_result.aperture_area,zoom_result.register_area[0],zoom_result.register_area[1],
          zoom_result.register_area[2],wide_result.aperture_area);
    }
}

int main(int argc,char** argv){
    if(argc<2||argc>3) return 2;
    const std::string mode=argv[1];
    Textures glass;
    Photos photos;
    pane_photos(photos,glass);
    if(mode=="panes"){
        for(int p=0;p<3;++p){
            const std::string path="glass-face-"+std::to_string(p)+".png";
            if(cairo_surface_write_to_png(photos.panes[p],path.c_str())!=CAIRO_STATUS_SUCCESS) return 1;
        }
        return 0;
    }
    if(mode=="proof"){
        for(int season=0;season<3;++season){
            const auto sun=sun_vector(declinations[season],12);
            const auto rays=floor_rays(sun,97);
            const auto bounds=patch_bounds(sun);
            const double x=motif_x[season],y=noon_landing(season);
            auto* image=cairo_image_surface_create(CAIRO_FORMAT_RGB24,820,820);
            auto stat=floor(image,glass,rays,bounds,{x-.55,x+.55,y-.55,y+.55},std::sqrt(sun[2]),.74,97);
            auto audit=floor(photos.room,glass,rays,bounds,hall,std::sqrt(sun[2]),.74,97);
            for(int other=0;other<3;++other) if(other!=season && audit.register_area[other]>.0002) return 1;
            std::printf("%d landing %.3fm overlap %.4f m2 triple register %.5f %.5f %.5f m2\n",
                   season,y,audit.aperture_area,audit.register_area[0],audit.register_area[1],audit.register_area[2]);
            const std::string path="layered-noon-"+std::to_string(season)+".png";
            auto saved=cairo_surface_write_to_png(image,path.c_str());
            if(season==1){
                auto collapsed=floor(image,glass,floor_rays(sun,97,0),{-10,10,0,16,true},
                                     {x-.55,x+.55,y-.55,y+.55},std::sqrt(sun[2]),.74,97);
                std::printf("Equinox with gaps closed triple register %.5f m2\n",collapsed.register_area[1]);
                if(collapsed.register_area[1]>.0001 ||
                   cairo_surface_write_to_png(image,"layered-no-gaps.png")!=CAIRO_STATUS_SUCCESS) return 1;
            }
            cairo_surface_destroy(image);
            if(saved!=CAIRO_STATUS_SUCCESS||stat.register_area[season]<.035) return 1;
        }
        return 0;
    }

    const int season=std::stoi(mode);
    if(season<0||season>2) return 2;
    const int still=argc==3?std::stoi(argv[2]):-1;
    if(still>=count) return 2;
    double previous=-1;
    for(int n=0;n<count;++n) {
        double h=clock_hour(12.*n/(count-1),sunrise(declinations[season]));
        if(h<=previous||(n==0&&h!=0)||(n==count-1&&h!=24)) return 1;
        previous=h;
    }
    auto* canvas=cairo_image_surface_create(CAIRO_FORMAT_RGB24,width,height);
    auto* cr=cairo_create(canvas);
    for(int n=still>=0?still:0;n<(still>=0?still+1:count);++n){
        composition(cr,photos,glass,season,n);
        cairo_surface_flush(canvas);
        if(still>=0) {
            const std::string path="layered-motion-"+std::to_string(season)+"-"+std::to_string(still)+".png";
            if(cairo_surface_write_to_png(canvas,path.c_str())!=CAIRO_STATUS_SUCCESS) return 1;
        } else {
            const size_t bytes=height*cairo_image_surface_get_stride(canvas);
            if(std::fwrite(cairo_image_surface_get_data(canvas),1,bytes,stdout)!=bytes) return 1;
        }
    }
    cairo_destroy(cr);
    cairo_surface_destroy(canvas);
    return 0;
}
