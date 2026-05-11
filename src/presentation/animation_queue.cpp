// src/presentation/animation_queue.cpp
// 动画队列引擎实现 - 非阻塞状态机

#include "animation_queue.h"
#include <Arduino.h>

AnimationQueue animQueue;

// ============================================================================
//  初始化
// ============================================================================

void AnimationQueue::init() {
    _head = 0;
    _tail = 0;
    _count = 0;
    _playing = false;
    _waitingInput = false;
    _nodeStartMs = 0;
    _onQueueComplete = nullptr;
    _onNodeChange = nullptr;
}

// ============================================================================
//  入队/清空
// ============================================================================

bool AnimationQueue::enqueue(const AnimNode& node) {
    if (_count >= ANIM_QUEUE_CAPACITY) return false;
    _buffer[_tail] = node;
    _tail = (_tail + 1) % ANIM_QUEUE_CAPACITY;
    _count++;

    // 如果队列之前为空且未在播放，立即开始
    if (!_playing && _count == 1) {
        startCurrentNode(millis());
    }
    return true;
}

void AnimationQueue::clear() {
    // 如果当前节点有 onComplete 回调，不触发（强制清空）
    _head = 0;
    _tail = 0;
    _count = 0;
    _playing = false;
    _waitingInput = false;
}

// ============================================================================
//  状态查询
// ============================================================================

bool AnimationQueue::isEmpty() const {
    return _count == 0;
}

bool AnimationQueue::isPlaying() const {
    return _playing;
}

bool AnimationQueue::isWaitingInput() const {
    return _waitingInput;
}

uint8_t AnimationQueue::size() const {
    return _count;
}

const AnimNode* AnimationQueue::currentNode() const {
    if (!_playing || _count == 0) return nullptr;
    return &_buffer[_head];
}

uint32_t AnimationQueue::currentElapsedMs() const {
    if (!_playing) return 0;
    return millis() - _nodeStartMs;
}

uint8_t AnimationQueue::currentFrameIndex(uint16_t frameDelayMs) const {
    if (!_playing || frameDelayMs == 0) return 0;
    uint32_t elapsed = millis() - _nodeStartMs;
    return (uint8_t)(elapsed / frameDelayMs);
}

// ============================================================================
//  状态机驱动
// ============================================================================

void AnimationQueue::update(uint32_t nowMs) {
    if (!_playing || _count == 0) return;

    // 等待输入时不自动推进
    if (_waitingInput) return;

    const AnimNode& node = _buffer[_head];
    uint16_t duration = getNodeDuration(node);

    // 检查当前节点是否播放完毕
    if (duration > 0 && (nowMs - _nodeStartMs) >= duration) {
        advanceToNext(nowMs);
    }
}

// ============================================================================
//  外部控制
// ============================================================================

void AnimationQueue::resumeFromInput() {
    if (!_waitingInput) return;
    _waitingInput = false;
    // 等待输入节点完成，推进到下一个
    advanceToNext(millis());
}

void AnimationQueue::skipCurrent() {
    if (!_playing || _count == 0) return;
    if (_waitingInput) return;  // 等待输入节点不可跳过
    advanceToNext(millis());
}

// ============================================================================
//  回调设置
// ============================================================================

void AnimationQueue::setOnQueueComplete(QueueCompleteCallback cb) {
    _onQueueComplete = cb;
}

void AnimationQueue::setOnNodeChange(NodeChangeCallback cb) {
    _onNodeChange = cb;
}

// ============================================================================
//  内部方法
// ============================================================================

void AnimationQueue::startCurrentNode(uint32_t nowMs) {
    if (_count == 0) {
        _playing = false;
        return;
    }

    _playing = true;
    _nodeStartMs = nowMs;

    const AnimNode& node = _buffer[_head];

    // 检查是否是等待输入节点
    if (node.type == NODE_WAIT_INPUT ||
        node.type == NODE_UI_FEED_CARDS ||
        node.type == NODE_UI_COMBO_SELECT) {
        _waitingInput = true;
    } else {
        _waitingInput = false;
    }

    // 触发 onStart 回调
    if (node.onStart) {
        node.onStart();
    }

    // 通知节点切换
    if (_onNodeChange) {
        _onNodeChange(&node, nullptr);
    }
}

void AnimationQueue::advanceToNext(uint32_t nowMs) {
    if (_count == 0) return;

    const AnimNode& finishedNode = _buffer[_head];

    // 触发 onComplete 回调
    if (finishedNode.onComplete) {
        finishedNode.onComplete();
    }

    // 出队
    const AnimNode* prev = &_buffer[_head];
    _head = (_head + 1) % ANIM_QUEUE_CAPACITY;
    _count--;

    if (_count == 0) {
        // 队列清空
        _playing = false;
        _waitingInput = false;
        if (_onNodeChange) {
            _onNodeChange(nullptr, prev);
        }
        if (_onQueueComplete) {
            _onQueueComplete();
        }
    } else {
        // 开始下一个节点
        const AnimNode* next = &_buffer[_head];
        if (_onNodeChange) {
            _onNodeChange(next, prev);
        }
        startCurrentNode(nowMs);
    }
}

uint16_t AnimationQueue::getNodeDuration(const AnimNode& node) const {
    // 如果节点指定了时长，使用它
    if (node.durationMs > 0) return node.durationMs;

    // 等待输入节点没有固定时长
    if (node.type == NODE_WAIT_INPUT ||
        node.type == NODE_UI_FEED_CARDS ||
        node.type == NODE_UI_COMBO_SELECT) {
        return 0;
    }

    // 回调节点瞬间完成
    if (node.type == NODE_CALLBACK) return 1;

    // 默认时长 (资源加载后可由 asset_loader 提供精确值)
    // 这里给一个安全默认值，实际应由 director 在入队时设置
    return 500;
}
