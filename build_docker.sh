IMAGE_NAME="pytorch-2-11-dev"
TAG="latest"
DOCKERFILE="docker/Dockerfile.pytorch"

echo "🔨 Building Docker image using $DOCKERFILE..."

docker build -f $DOCKERFILE -t $IMAGE_NAME:$TAG .

echo "✅ Build finished: $IMAGE_NAME:$TAG"