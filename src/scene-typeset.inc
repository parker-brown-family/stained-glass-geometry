void source(cairo_t* cr, Rgb rgb, double alpha=1) {
    cairo_set_source_rgba(cr,rgb[0],rgb[1],rgb[2],alpha);
}
void rect(cairo_t* cr,double x,double y,double w,double h,Rgb color,double alpha=1) {
    source(cr,color,alpha);
    cairo_rectangle(cr,x,y,w,h);
    cairo_fill(cr);
}
void rounded(cairo_t* cr,double x,double y,double w,double h,double r) {
    cairo_new_sub_path(cr);
    cairo_arc(cr,x+w-r,y+r,r,-pi/2,0);
    cairo_arc(cr,x+w-r,y+h-r,r,0,pi/2);
    cairo_arc(cr,x+r,y+h-r,r,pi/2,pi);
    cairo_arc(cr,x+r,y+r,r,pi,3*pi/2);
    cairo_close_path(cr);
}
void stroke(cairo_t* cr,Point a,Point b,Rgb color,double weight=1,double alpha=1) {
    source(cr,color,alpha);
    cairo_set_line_width(cr,weight);
    cairo_move_to(cr,a.x,a.y);
    cairo_line_to(cr,b.x,b.y);
    cairo_stroke(cr);
}
void orb(cairo_t* cr,Point at,double radius,Rgb color,double alpha=1) {
    source(cr,color,alpha);
    cairo_arc(cr,at.x,at.y,radius,0,2*pi);
    cairo_fill(cr);
}
void halo(cairo_t* cr,Point at,double radius,Rgb color,double alpha=1) {
    auto* gradient=cairo_pattern_create_radial(at.x,at.y,0,at.x,at.y,radius);
    cairo_pattern_add_color_stop_rgba(gradient,0,color[0],color[1],color[2],alpha);
    cairo_pattern_add_color_stop_rgba(gradient,1,color[0],color[1],color[2],0);
    cairo_set_source(cr,gradient);
    cairo_arc(cr,at.x,at.y,radius,0,2*pi);
    cairo_fill(cr);
    cairo_pattern_destroy(gradient);
}
void text(cairo_t* cr,Point at,const std::string& words,double size,Rgb color,
          const char* family="Adwaita Sans",int weight=500,double anchor=0,double spacing=0) {
    auto* layout=pango_cairo_create_layout(cr);
    auto* description=pango_font_description_new();
    pango_font_description_set_family(description,family);
    pango_font_description_set_absolute_size(description,size*PANGO_SCALE);
    pango_font_description_set_weight(description,static_cast<PangoWeight>(weight));
    pango_layout_set_font_description(layout,description);
    pango_layout_set_text(layout,words.c_str(),-1);
    if (spacing!=0) {
        auto* attributes=pango_attr_list_new();
        pango_attr_list_insert(attributes,pango_attr_letter_spacing_new(spacing*PANGO_SCALE));
        pango_layout_set_attributes(layout,attributes);
        pango_attr_list_unref(attributes);
    }
    int w,h;
    pango_layout_get_pixel_size(layout,&w,&h);
    source(cr,color);
    cairo_move_to(cr,at.x-anchor*w,at.y);
    pango_cairo_show_layout(cr,layout);
    pango_font_description_free(description);
    g_object_unref(layout);
}
void micro(cairo_t* cr,Point at,const std::string& words,Rgb color=quiet,double anchor=0) {
    text(cr,at,words,20,color,"Adwaita Sans",630,anchor,2.3);
}
