#ifndef BOX_H
#define BOX_H
#endif


#include "caffe/common.hpp"
#include "caffe/proto/caffe.pb.h"
#include <cfloat>
#include "opencv2/opencv.hpp"
using namespace cv;
using namespace std;


namespace caffe {

typedef struct{
    float x, y, w, h;
} box;

typedef struct{
    float dx, dy, dw, dh;
} dbox;

//template <typename Dtype>
box float_to_box(const double *f);

box float_to_box(const float *f);

float box_iou(box a, box b);


float box_rmse(box a, box b);

dbox diou(box a, box b);


void do_nms(box *boxes, float **probs, int total, int classes, float thresh);


void do_nms_sort(box *boxes, float **probs, int total, int classes, float thresh);


void do_nms_obj(box *boxes, float **probs, int total, int classes, float thresh);


box decode_box(box b, box anchor);


box encode_box(box b, box anchor);


// region loss functions
template <typename Dtype>
static inline Dtype logistic_activate(Dtype x){return 1./(1. + exp(-x));}

template <typename Dtype>
static inline Dtype logistic_gradient(Dtype x){return (1-x)*x;}


template <typename Dtype>
void flatten(Dtype *x, int size, int layers, int batch, int forward);

template <typename Dtype>
void softmax(Dtype *input, int n, Dtype temp, Dtype *output)
{
    int i;
    Dtype sum = 0;
    Dtype largest = -FLT_MAX;
    for(i = 0; i < n; ++i){
        if(input[i] > largest) largest = input[i];
    }
    for(i = 0; i < n; ++i){
        Dtype e = exp(input[i]/temp - largest/temp);
        sum += e;
        output[i] = e;
    }
    for(i = 0; i < n; ++i){
        output[i] /= sum;
    }
}


template <typename Dtype>
box get_region_box(Dtype *x, Dtype *biases, int n, int index, int i, int j, int w, int h)
{
    int stride = w * h;
    box b;
    b.x = (i + logistic_activate(x[index + 0 * stride])) / w;
    b.y = (j + logistic_activate(x[index + 1 * stride])) / h;
    b.w = exp(x[index + 2 * stride]) * biases[2*n]   / w;
    b.h = exp(x[index + 3 * stride]) * biases[2*n+1] / h;
    return b;
}
template <typename Dtype>
box get_yolo_box(Dtype *x, vector<float>& biases, int n, int index, int i, int j, int w, int h, int nw, int nh)
{
    int stride = w * h;
    box b;
    b.x = (i + x[index + 0 * stride]) / w;
    b.y = (j + x[index + 1 * stride]) / h;
    b.w = exp(x[index + 2 * stride]) * biases[2*n]   / nw;
    b.h = exp(x[index + 3 * stride]) * biases[2*n+1] / nh;
    return b;
}
template <typename Dtype>
float delta_region_box(box truth, Dtype *x, Dtype *biases, int n, int index, int i, int j, int w, int h, Dtype *delta, float scale, Dtype &coord_loss, Dtype &area_loss)
{
    int stride = w * h;
    box pred = get_region_box(x, biases, n, index, i, j, w, h);
    float iou = box_iou(pred, truth);

    Dtype tx = (truth.x*w - i);
    Dtype ty = (truth.y*h - j);
    Dtype tw = log(truth.w*w / biases[2*n]);
    Dtype th = log(truth.h*h / biases[2*n + 1]);

    delta[index + 0 * stride] = (-1) * scale * (tx - logistic_activate(x[index + 0 * stride])) * logistic_gradient(logistic_activate(x[index + 0 * stride]));
    delta[index + 1 * stride] = (-1) * scale * (ty - logistic_activate(x[index + 1 * stride])) * logistic_gradient(logistic_activate(x[index + 1 * stride]));
    delta[index + 2 * stride] = (-1) * scale * (tw - x[index + 2 * stride]);
    delta[index + 3 * stride] = (-1) * scale * (th - x[index + 3 * stride]);


//    std::cout<<"delta coord: "<<delta[index + 0]<<" "<<delta[index + 1]<<" "<<delta[index + 2]<<" "<<delta[index + 3]<<std::endl;

    coord_loss += scale * (pow((float)((tx-logistic_activate(x[index + 0 * stride]))), 2) + pow((float)((ty - logistic_activate(x[index + 1 * stride]))), 2));
    area_loss += scale * (pow((float)((tw - x[index + 2 * stride])), 2) + pow((float)((th - x[index + 3 * stride])), 2));
    return iou;
}
template <typename Dtype>
float delta_yolo_box(box truth, Dtype *x, vector<float>& biases, int n, int index, int i, int j, int w, int h, int nw, int nh, Dtype *delta, float scale, Dtype &coord_loss, Dtype &area_loss)
{
    int stride = w * h;
    box pred = get_yolo_box(x, biases, n, index, i, j, w, h, nw, nh);
    float iou = box_iou(pred, truth);

    Dtype tx = (truth.x*w - i);
    Dtype ty = (truth.y*h - j);
    Dtype tw = log(truth.w*nw / biases[2*n]);
    Dtype th = log(truth.h*nh / biases[2*n + 1]);

    delta[index + 0 * stride] = (-1) * scale * (tx - x[index + 0 * stride]);
    delta[index + 1 * stride] = (-1) * scale * (ty - x[index + 1 * stride]);
    delta[index + 2 * stride] = (-1) * scale * (tw - x[index + 2 * stride]);
    delta[index + 3 * stride] = (-1) * scale * (th - x[index + 3 * stride]);

//    std::cout<<"delta coord: "<<delta[index + 0]<<" "<<delta[index + 1]<<" "<<delta[index + 2]<<" "<<delta[index + 3]<<std::endl;

    coord_loss += 1.0 * (pow((float)((tx - x[index + 0 * stride])), 2) + pow((float)((ty - x[index + 1 * stride])), 2));
    area_loss += 1.0 * (pow((float)((tw - x[index + 2 * stride])), 2) + pow((float)((th - x[index + 3 * stride])), 2));
    return iou;
}
template <typename Dtype>
float diff_iou_loss2(box& truth, Dtype *x, vector<float>& biases, int n, int index, int i, int j, int w, int h, Dtype *delta, float scale, Dtype &iou_loss, int nw = 0, int nh = 0)
{
    int stride = w * h;
    box pred = get_yolo_box(x, biases, n, index, i, j, w, h, nw, nh);
    box p = pred, t=truth;
    float iou = box_iou(pred, truth);
    
    Dtype X_p = p.w * p.h;
    Dtype X_t = t.w * t.h;
    Dtype X = (X_p + X_t) / 2;
    Dtype Bw = std::max(p.x+p.w/2, t.x+t.w/2) - std::min(p.x-p.w/2, t.x-t.w/2);
    Dtype Bh = std::max(p.y+p.h/2, t.y+t.h/2) - std::min(p.y-p.h/2, t.y-t.h/2);
    float aob = X / (Bw * Bh);

    Dtype dp_ox = logistic_gradient(logistic_activate(x[index + 0 * stride])) / w;
    Dtype dp_oy = logistic_gradient(logistic_activate(x[index + 1 * stride])) / h;
    Dtype dp_ow = exp(x[index + 2 * stride]) * biases[2*n] / nw;
    Dtype dp_oh = exp(x[index + 3 * stride]) * biases[2*n+1] / nh;

    Dtype dX_pw = p.h / 2;
    Dtype dX_ph = p.w / 2 ;

    Dtype dBw_px = 0, dBw_pw = 0;
    if(p.x+p.w/2>t.x+t.w/2 &&  p.x-p.w/2>t.x-t.w/2)
    {
        dBw_px = 1.0, dBw_pw = 1.0/2;
    }
    else if(p.x+p.w/2>t.x+t.w/2 &&  p.x-p.w/2<t.x-t.w/2)
    {
        dBw_px = 0.0, dBw_pw = 1.0;
    }
    else if(p.x+p.w/2<t.x+t.w/2 &&  p.x-p.w/2<t.x-t.w/2)
    {
        dBw_px = -1.0, dBw_pw = 1.0/2;
    }
    else if(p.x+p.w/2<t.x+t.w/2 &&  p.x-p.w/2>t.x-t.w/2)
    {
        dBw_px = 0.0, dBw_pw = 0.0;
    }


    Dtype dBh_py = 0, dBh_ph = 0;
    if(p.y+p.h/2>t.y+t.h/2 && p.y-p.h/2>t.y-t.h/2)
    {
        dBh_py = 1.0, dBh_ph = 1.0/2;
    }
    else if(p.y+p.h/2>t.y+t.h/2 && p.y-p.h/2<t.y-t.h/2)
    {
        dBh_py = 0.0, dBh_ph = 1.0;
    }
    else if(p.y+p.h/2<t.y+t.h/2 && p.y-p.h/2<t.y-t.h/2)
    {
        dBh_py = -1.0, dBh_ph = 1.0/2;
    }
    else if(p.y+p.h/2<t.y+t.h/2 && p.y-p.h/2>t.y-t.h/2)
    {
        dBh_py = 0.0, dBh_ph = 0.0;
    }

    Dtype dL_aob = (Bw * Bh) / X;
    Dtype B = Bw * Bh;
    Dtype B_2 = Bw * Bh * Bw * Bh;
    Dtype dAoB_px = - X / B_2 * dBw_px * Bh;
    Dtype dAoB_py = - X / B_2 * Bw * dBh_py;
    Dtype dAoB_pw = dX_pw / B - X / B_2 * dBw_pw * Bh;
    Dtype dAoB_ph = dX_ph / B - X / B_2 * Bw * dBh_ph;

    delta[index + 0 * stride] = (-1) * scale * dL_aob * dAoB_px * dp_ox;
    delta[index + 1 * stride] = (-1) * scale * dL_aob * dAoB_py * dp_oy;
    delta[index + 2 * stride] = (-1) * scale * dL_aob * dAoB_pw * dp_ow;
    delta[index + 3 * stride] = (-1) * scale * dL_aob * dAoB_ph * dp_oh;

    /*
    std::cout<<"aob:  "<<aob<<std::endl;
    std::cout<<"output  x y w h: "<<x[index + 0 * stride]<<" "
            <<x[index + 1 * stride]<<" "<<x[index + 2 * stride]<<" "<<x[index + 3 * stride]<<std::endl;
    std::cout<<"predict x y w h: "<<p.x<<" "<<p.y<<" "<<p.w<<" "<<p.h<<std::endl;
    std::cout<<"truth   x y w h: "<<t.x<<" "<<t.y<<" "<<t.w<<" "<<t.h<<std::endl;
    std::cout<<"dAoB_px dAoB_py dAoB_pw dAoB_ph: "<<delta[index + 0 * stride]<<" "<<delta[index + 1 * stride]
            <<" "<<delta[index + 2 * stride]<<" "<<delta[index + 3 * stride]<<std::endl;
    */
    iou_loss += - scale *log(aob);

    return iou;
}
template <typename Dtype>
void delta_region_class(Dtype *output, Dtype *delta, int index, int class_ind, int classes, float scale, Dtype &avg_cat, Dtype &class_loss, int stride)
{
    int n;
    
    for(n = 0; n < classes; ++n){
        delta[index + n * stride] = (-1) * scale * (((n == class_ind)?1 : 0) - output[index + n * stride]);
        class_loss += scale * pow((float)((((n == class_ind)?1 : 0) - output[index + n * stride])), 2);
        if(n == class_ind) avg_cat += output[index + n * stride];

    }
    
}

template <typename Dtype>
void delta_yolo_class(Dtype *output, Dtype *delta, int index, int class_ind, int classes, float scale, Dtype &avg_cat, Dtype &class_loss, int stride)
{
    int n;
    /*
    if(delta[index]) {
        delta[index + stride*classes] = 1 - output[index + stride*classes];
        avg_cat += output[index + stride*classes];
        return;
    }
    */
    for(n = 0; n < classes; ++n){
        delta[index + n * stride] = (-1) * scale * (((n == class_ind)?1 : 0) - output[index + n * stride]);
        class_loss += 1.0 * pow((float)((((n == class_ind)?1 : 0) - output[index + n * stride])), 2);
        if(n == class_ind) avg_cat += output[index + n * stride];

    }
    
}
int int_index(vector<int>& a, int val, int n);
int entry_index(int side_w, int side_h, int coords, int classes, int location, int entry);

template <typename Dtype>
void get_region_boxes(int side_w, int side_h, Dtype *biases, int box_num, int cls_num, Dtype* pRes, float thresh, float** probs, box *boxes, int only_objectness)
{
    int locations = side_w * side_h;
    for (int i = 0; i < locations; ++i){
        int row = i / side_w;
        int col = i % side_w;
        for(int n = 0; n < box_num; ++n){
            int index = n*locations + i;
            for(int j = 0; j < cls_num; ++j){
                probs[index][j] = 0;
            }
            int obj_index  = entry_index(side_w, side_h, 4, 0, n*locations + i, 4);
            int box_index  = entry_index(side_w, side_h, 4, 0, n*locations + i, 0);
            float scale = logistic_activate(pRes[obj_index]) ;
            probs[index][0] = scale > thresh ? scale : 0;
            if(scale > thresh)
              boxes[index] = get_region_box(pRes, biases, n, box_index, col, row, side_w, side_h);
        }
    }
}

template <typename Dtype>
int max_index(Dtype *a, int n);

void get_valid_boxes(int batch_ind, vector< vector<float> > &pred_box, box *boxes, float **probs, int num, int classes);

void draw_boxes(cv::Mat &img, vector<box> &boxs_v, cv::Scalar color);

void get_predict_boxes(int batch_ind, vector< vector<float> > &pred_box, box *boxes, float **probs, int num, int classes);


template <typename Dtype>
void get_predict_boxes(int batch_ind, int side_w, int side_h, Dtype *biases, int box_num, int cls_num, Dtype* pRes,
  vector< vector<float> > &pred_boxes, float thresh)
{
    int locations = side_w * side_h;
    box *boxes = (box *)calloc(locations*box_num, sizeof(box));
    float **probs = (float **)calloc(locations*box_num, sizeof(float *));
    for(int j = 0; j < locations*box_num; ++j) probs[j] = (float *)calloc(cls_num+1, sizeof(float));
    get_region_boxes(side_w, side_h, biases, box_num, cls_num, pRes, thresh, probs, boxes, 1);
    do_nms_obj(boxes, probs, side_w*side_h*box_num, cls_num, 0.4);
    get_predict_boxes(batch_ind, pred_boxes, boxes, probs, side_w*side_h*box_num, cls_num);
    for(int j = 0; j < locations*box_num; ++j) free(probs[j]);
    free(probs);
    free(boxes);
}

bool mycomp(vector<float> &a,vector<float> &b);

}

