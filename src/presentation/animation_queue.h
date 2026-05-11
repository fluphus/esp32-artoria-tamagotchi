// src/presentation/animation_queue.h
// 动画队列引擎 - 非阻塞、基于 millis() 的状态机
// 纯机械模块，不含任何业务逻辑判定

#ifndef ANIMATION_QUEUE_H
#define ANIMATION_QUEUE_H

#include <stdint.h>
#include "presentation_types.h"

// ============================================================================
//  AnimationQueue - 环形缓冲区实现的动画节点队列
// ============================================================================

class AnimationQueue {
public:
    void init();

    // --- 入队 ---
    bool enqueue(const AnimNode& node);     // 返回 false = 队列已满
    void clear();                           // 清空队列并停止播放

    // --- 状态查询 ---
    bool isEmpty() const;
    bool isPlaying() const;
    bool isWaitingInput() const;            // 当前节点是等待输入类型
    uint8_t size() const;
    uint8_t capacity() const { return ANIM_QUEUE_CAPACITY; }

    // --- 当前节点信息 (供渲染层查询) ---
    const AnimNode* currentNode() const;    // nullptr = 队列空/未播放
    uint32_t currentElapsedMs() const;
    uint8_t currentFrameIndex(uint16_t frameDelayMs) const;  // 根据帧延迟计算当前帧

    // --- 状态机驱动 (每帧由主循环调用) ---
    void update(uint32_t nowMs);

    // --- 外部控制 ---
    void resumeFromInput();                 // 恢复被 WAIT_INPUT 暂停的队列
    void skipCurrent();                     // 跳过当前节点 (如果 skippable)

    // --- 队列完成回调 ---
    typedef void (*QueueCompleteCallback)();
    void setOnQueueComplete(QueueCompleteCallback cb);

    // --- 节点切换回调 (供 DisplayManager 同步渲染状态) ---
    typedef void (*NodeChangeCallback)(const AnimNode* newNode, const AnimNode* prevNode);
    void setOnNodeChange(NodeChangeCallback cb);

private:
    AnimNode _buffer[ANIM_QUEUE_CAPACITY];
    uint8_t _head;          // 队首索引 (下一个要消费的)
    uint8_t _tail;          // 队尾索引 (下一个入队位置)
    uint8_t _count;         // 当前队列中的节点数

    // 播放状态
    bool _playing;
    bool _waitingInput;
    uint32_t _nodeStartMs;  // 当前节点开始时间

    // 回调
    QueueCompleteCallback _onQueueComplete;
    NodeChangeCallback _onNodeChange;

    // 内部方法
    void startCurrentNode(uint32_t nowMs);
    void advanceToNext(uint32_t nowMs);
    uint16_t getNodeDuration(const AnimNode& node) const;
};

// 全局单例
extern AnimationQueue animQueue;

#endif // ANIMATION_QUEUE_H
