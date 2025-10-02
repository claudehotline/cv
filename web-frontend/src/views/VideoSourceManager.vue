<template>
  <div class="video-source-manager">
    <el-row :gutter="20">
      <!-- 视频源管理面板 -->
      <el-col :span="16">
        <el-card shadow="hover">
          <template #header>
            <div class="card-header">
              <span>视频源管理</span>
              <el-button size="small" type="primary" @click="handleAddSource">
                添加视频源
              </el-button>
            </div>
          </template>

          <el-table
            :data="videoStore.videoSources"
            size="medium"
            style="width: 100%"
          >
            <el-table-column prop="name" label="名称" width="150" />
            <el-table-column prop="type" label="类型" width="100">
              <template #default="{ row }">
                <el-tag
                  :type="
                    row.type === 'camera'
                      ? 'success'
                      : row.type === 'file'
                        ? 'info'
                        : 'warning'
                  "
                  size="small"
                >
                  {{ getTypeLabel(row.type) }}
                </el-tag>
              </template>
            </el-table-column>
            <el-table-column
              prop="url"
              label="地址/路径"
              width="250"
              show-overflow-tooltip
            />
            <el-table-column prop="fps" label="帧率" width="80" />
            <el-table-column prop="resolution" label="分辨率" width="120" />
            <el-table-column prop="status" label="状态" width="100">
              <template #default="{ row }">
                <el-tag
                  :type="
                    row.status === 'active'
                      ? 'success'
                      : row.status === 'inactive'
                        ? 'info'
                        : 'danger'
                  "
                  size="small"
                >
                  {{ getStatusLabel(row.status) }}
                </el-tag>
              </template>
            </el-table-column>
            <el-table-column label="操作" width="200">
              <template #default="{ row }">
                <el-button size="small" type="primary" @click="editSource(row)">
                  编辑
                </el-button>
                <el-button
                  size="small"
                  type="success"
                  @click="selectSource(row.id)"
                >
                  选择
                </el-button>
                <el-button
                  size="small"
                  type="danger"
                  @click="removeSource(row.id)"
                >
                  删除
                </el-button>
              </template>
            </el-table-column>
          </el-table>
        </el-card>
      </el-col>

      <!-- 连接状态面板 -->
      <el-col :span="8">
        <el-card shadow="hover">
          <template #header>
            <span>系统状态</span>
          </template>

          <div class="status-panel">
            <div class="connection-status">
              <el-alert
                :type="connectionStatusType"
                :title="connectionStatusText"
                :closable="false"
                size="small"
              />
            </div>

            <div v-if="videoStore.selectedSource" class="selected-source">
              <h4>当前选择的视频源</h4>
              <el-descriptions :column="1" border size="small">
                <el-descriptions-item label="名称">
                  {{ videoStore.selectedSource.name }}
                </el-descriptions-item>
                <el-descriptions-item label="类型">
                  <el-tag
                    :type="
                      videoStore.selectedSource.type === 'camera'
                        ? 'success'
                        : videoStore.selectedSource.type === 'file'
                          ? 'info'
                          : 'warning'
                    "
                    size="small"
                  >
                    {{ getTypeLabel(videoStore.selectedSource.type) }}
                  </el-tag>
                </el-descriptions-item>
                <el-descriptions-item label="地址">
                  {{ videoStore.selectedSource.url }}
                </el-descriptions-item>
                <el-descriptions-item label="状态">
                  <el-tag
                    :type="
                      videoStore.selectedSource.status === 'active'
                        ? 'success'
                        : videoStore.selectedSource.status === 'inactive'
                          ? 'info'
                          : 'danger'
                    "
                    size="small"
                  >
                    {{ getStatusLabel(videoStore.selectedSource.status) }}
                  </el-tag>
                </el-descriptions-item>
              </el-descriptions>

              <div style="margin-top: 15px">
                <el-button type="primary" @click="goToAnalysis">
                  前往分析页面
                </el-button>
              </div>
            </div>

            <div v-else class="no-selection">
              <el-empty description="请选择一个视频源" />
            </div>
          </div>
        </el-card>
      </el-col>
    </el-row>

    <!-- 添加/编辑视频源对话框 -->
    <el-dialog
      v-model="sourceDialogVisible"
      :title="isEditing ? '编辑视频源' : '添加视频源'"
      width="500px"
    >
      <el-form ref="sourceFormRef" :model="currentSource" label-width="100px">
        <el-form-item label="源名称" required>
          <el-input v-model="currentSource.name" placeholder="请输入源名称" />
        </el-form-item>
        <el-form-item label="源类型" required>
          <el-select
            v-model="currentSource.type"
            placeholder="请选择类型"
            style="width: 100%"
          >
            <el-option label="摄像头" value="camera" />
            <el-option label="视频文件" value="file" />
            <el-option label="网络流" value="stream" />
          </el-select>
        </el-form-item>
        <el-form-item label="源地址" required>
          <el-input v-model="currentSource.url" placeholder="请输入地址" />
        </el-form-item>
        <el-form-item label="帧率">
          <el-input-number v-model="currentSource.fps" :min="1" :max="60" />
        </el-form-item>
        <el-form-item label="分辨率">
          <el-input
            v-model="currentSource.resolution"
            placeholder="如: 1280x720"
          />
        </el-form-item>
      </el-form>

      <template #footer>
        <el-button @click="sourceDialogVisible = false">取消</el-button>
        <el-button type="primary" @click="saveVideoSource">确定</el-button>
      </template>
    </el-dialog>
  </div>
</template>

<script setup lang="ts">
import { ref, computed, onMounted } from "vue";
import { useRouter } from "vue-router";
import { useVideoStore } from "@/stores/videoStore";
import type { VideoSource } from "@/types";

const router = useRouter();
const videoStore = useVideoStore();

// 数据
const sourceDialogVisible = ref(false);
const addSourceDialogVisible = ref(false);
const isEditing = ref(false);
const sourceFormRef = ref();
const currentSource = ref({
  id: "",
  name: "",
  type: "camera" as const,
  url: "",
  fps: 30,
  resolution: "1280x720",
  status: "inactive" as const,
});

// 标签映射
type VideoSourceType = VideoSource["type"];
type VideoSourceStatus = VideoSource["status"];

const typeLabels: Record<VideoSourceType, string> = {
  camera: "\u6444\u50cf\u5934",
  file: "\u6587\u4ef6",
  stream: "\u6d41",
};

const statusLabels: Record<VideoSourceStatus, string> = {
  active: "\u8fd0\u884c\u4e2d",
  inactive: "\u672a\u6fc0\u6d3b",
  error: "\u9519\u8bef",
};

const getTypeLabel = (type: VideoSourceType) => typeLabels[type];
const getStatusLabel = (status: VideoSourceStatus) => statusLabels[status];

const connectionStatusType = computed(() => {
  switch (videoStore.connectionStatus) {
    case "connected":
      return "success";
    case "connecting":
      return "warning";
    default:
      return "error";
  }
});

const connectionStatusText = computed(() => {
  switch (videoStore.connectionStatus) {
    case "connected":
      return "已连接到后端服务";
    case "connecting":
      return "正在连接后端服务...";
    default:
      return "后端服务连接失败";
  }
});

// 方法
const selectSource = (sourceId: string) => {
  videoStore.setSelectedSource(sourceId);
};

const editSource = (source: any) => {
  isEditing.value = true;
  currentSource.value = { ...source };
  sourceDialogVisible.value = true;
};

const removeSource = (sourceId: string) => {
  videoStore.removeVideoSource(sourceId);
};

const saveVideoSource = () => {
  if (isEditing.value) {
    videoStore.updateVideoSource(currentSource.value);
  } else {
    videoStore.addVideoSource(currentSource.value);
  }

  sourceDialogVisible.value = false;
  resetForm();
};

const resetForm = () => {
  currentSource.value = {
    id: "",
    name: "",
    type: "camera",
    url: "",
    fps: 30,
    resolution: "1280x720",
    status: "inactive",
  };
  isEditing.value = false;
};

const goToAnalysis = () => {
  router.push("/video-analysis");
};

// 监听添加按钮点击
const handleAddSource = () => {
  isEditing.value = false;
  resetForm();
  sourceDialogVisible.value = true;
};

// 生命周期
onMounted(async () => {
  console.log("🎬 VideoSourceManager组件已挂载");
  videoStore.init();
});
</script>

<style scoped>
.video-source-manager {
  height: 100%;
}

.card-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.status-panel {
  display: flex;
  flex-direction: column;
  gap: 20px;
}

.connection-status {
  margin-bottom: 15px;
}

.selected-source h4 {
  margin: 0 0 15px 0;
  color: #303133;
}

.no-selection {
  text-align: center;
  padding: 20px;
}
</style>
