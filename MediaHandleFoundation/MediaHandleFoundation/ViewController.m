//
//  ViewController.m
//  MediaHandleFoundation
//
//  Created by wiley on 2020/12/28.
//

#import "ViewController.h"
#include <libavutil/log.h>

@interface ViewController ()

@end

@implementation ViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    // Do any additional setup after loading the view.
    
    
    av_log(NULL, AV_LOG_ERROR, "error");
}


@end
