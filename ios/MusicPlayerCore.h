#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface MPCTrackInfo : NSObject
@property (nonatomic, copy)   NSString *title;
@property (nonatomic, copy)   NSString *artist;
@property (nonatomic, copy)   NSString *album;
@property (nonatomic, assign) double duration;
@property (nonatomic, strong, nullable) NSData *coverArt;   // bytes ảnh bìa gốc (JPEG/PNG...) lấy từ AV_DISPOSITION_ATTACHED_PIC
@end

/// Cầu nối Objective-C++ giữa Swift và lõi C++ (FFmpeg + FFT) trong thư mục core/.
@interface MusicPlayerCore : NSObject

@property (nonatomic, readonly) BOOL playing;
@property (nonatomic, readonly) BOOL ended;
@property (nonatomic, readonly) double position;
@property (nonatomic, readonly) double duration;
@property (nonatomic, readonly) NSInteger bandCount;
@property (nonatomic, readonly, strong) MPCTrackInfo *trackInfo;

- (BOOL)loadPath:(NSString *)path error:(NSError * _Nullable * _Nullable)error;
- (void)play;
- (void)pause;
- (void)seekToSeconds:(double)seconds;

/// Gọi từ audio render thread: điền PCM vào 2 buffer planar (L/R).
- (void)pullLeft:(float *)left right:(float *)right frames:(NSInteger)frames;

/// Gọi từ main thread (~60 Hz): ghi `count` giá trị 0..1 vào `bands`, trả về mức bass 0..1.
- (float)copySpectrum:(float *)bands count:(NSInteger)count;

@end

NS_ASSUME_NONNULL_END
