from .detector import Detector
import dlib
import face_recognition
import face_recognition_models
import numpy as np
import cv2
import os
import logging
logging.basicConfig(level = logging.INFO,format = '%(asctime)s - %(name)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

TMP_DIR = "./../faceswap/tmp"

mean_face_x = np.array([
0.000213256, 0.0752622, 0.18113, 0.29077, 0.393397, 0.586856, 0.689483, 0.799124,
0.904991, 0.98004, 0.490127, 0.490127, 0.490127, 0.490127, 0.36688, 0.426036,
0.490127, 0.554217, 0.613373, 0.121737, 0.187122, 0.265825, 0.334606, 0.260918,
0.182743, 0.645647, 0.714428, 0.793132, 0.858516, 0.79751, 0.719335, 0.254149,
0.340985, 0.428858, 0.490127, 0.551395, 0.639268, 0.726104, 0.642159, 0.556721,
0.490127, 0.423532, 0.338094, 0.290379, 0.428096, 0.490127, 0.552157, 0.689874,
0.553364, 0.490127, 0.42689 ])

mean_face_y = np.array([
0.106454, 0.038915, 0.0187482, 0.0344891, 0.0773906, 0.0773906, 0.0344891,
0.0187482, 0.038915, 0.106454, 0.203352, 0.307009, 0.409805, 0.515625, 0.587326,
0.609345, 0.628106, 0.609345, 0.587326, 0.216423, 0.178758, 0.179852, 0.231733,
0.245099, 0.244077, 0.231733, 0.179852, 0.178758, 0.216423, 0.244077, 0.245099,
0.780233, 0.745405, 0.727388, 0.742578, 0.727388, 0.745405, 0.780233, 0.864805,
0.902192, 0.909281, 0.902192, 0.864805, 0.784792, 0.778746, 0.785343, 0.778746,
0.784792, 0.824182, 0.831803, 0.824182 ])

landmarks_2D = np.stack( [ mean_face_x, mean_face_y ], axis=1 )

def umeyama(src, dst, estimate_scale):
    """Estimate N-D similarity transformation with or without scaling.
    Parameters
    ----------
    src : (M, N) array
        Source coordinates.
    dst : (M, N) array
        Destination coordinates.
    estimate_scale : bool
        Whether to estimate scaling factor.
    Returns
    -------
    T : (N + 1, N + 1)
        The homogeneous similarity transformation matrix. The matrix contains
        NaN values only if the problem is not well-conditioned.
    References
    ----------
    .. [1] "Least-squares estimation of transformation parameters between two
            point patterns", Shinji Umeyama, PAMI 1991, DOI: 10.1109/34.88573
    """

    num = src.shape[0]
    dim = src.shape[1]

    # Compute mean of src and dst.
    src_mean = src.mean(axis=0)
    dst_mean = dst.mean(axis=0)

    # Subtract mean from src and dst.
    src_demean = src - src_mean
    dst_demean = dst - dst_mean

    # Eq. (38).
    A = np.dot(dst_demean.T, src_demean) / num

    # Eq. (39).
    d = np.ones((dim,), dtype=np.double)
    if np.linalg.det(A) < 0:
        d[dim - 1] = -1

    T = np.eye(dim + 1, dtype=np.double)

    U, S, V = np.linalg.svd(A)

    # Eq. (40) and (43).
    rank = np.linalg.matrix_rank(A)
    if rank == 0:
        return np.nan * T
    elif rank == dim - 1:
        if np.linalg.det(U) * np.linalg.det(V) > 0:
            T[:dim, :dim] = np.dot(U, V)
        else:
            s = d[dim - 1]
            d[dim - 1] = -1
            T[:dim, :dim] = np.dot(U, np.dot(np.diag(d), V))
            d[dim - 1] = s
    else:
        T[:dim, :dim] = np.dot(U, np.dot(np.diag(d), V.T))

    if estimate_scale:
        # Eq. (41) and (42).
        scale = 1.0 / src_demean.var(axis=0).sum() * np.dot(S, d)
    else:
        scale = 1.0

    T[:dim, dim] = dst_mean - scale * np.dot(T[:dim, :dim], src_mean.T)
    T[:dim, :dim] *= scale

    return T

class Faceswap(Detector):
    def __init__(self, blur_size=2, seamless_clone=False, mask_type="facehullandrect", erosion_kernel_size=None, **kwargs):
        super(Faceswap, self).__init__()
        self.build_pipeline_by_JSONFile("./faceswap_config.json")
        predictor_68_point_model = face_recognition_models.pose_predictor_model_location()
        self.pose_predictor = dlib.shape_predictor(predictor_68_point_model)
        self.erosion_kernel = None
        self.blur_size = blur_size
        self.seamless_clone = seamless_clone
        self.mask_type = mask_type.lower()
        self.video_capture = None
        self.all_faces = []
    
    def preprocess(self, frame):
        faces = []
        face_locations = face_recognition.face_locations(frame)
        landmarks = self._raw_face_landmarks(frame, face_locations)
        for ((y, right, bottom, x), landmarks) in zip(face_locations, landmarks):
            faces.append(DetectedFace(frame[y: bottom, x: right], x, right - x, y, bottom - y, landmarks))
        
        return faces

    def get_align_mat(self, face):
        return umeyama(np.array(face.landmarksAsXY()[17:]), landmarks_2D, True)[0:2]

    def _raw_face_landmarks(self, face_image, face_locations):
        face_locations = [self._css_to_rect(face_location) for face_location in face_locations]
        return [self.pose_predictor(face_image, face_location) for face_location in face_locations]

    def _css_to_rect(self, css):
        return dlib.rectangle(css[3], css[0], css[1], css[2])

    def get_image_mask(self, image, new_face, face_detected, mat, image_size):

        face_mask = np.zeros(image.shape,dtype=float)
        if 'rect' in self.mask_type:
            face_src = np.ones(new_face.shape,dtype=float)
            cv2.warpAffine( face_src, mat, image_size, face_mask, cv2.WARP_INVERSE_MAP, cv2.BORDER_TRANSPARENT )

        hull_mask = np.zeros(image.shape,dtype=float)
        if 'hull' in self.mask_type:
            hull = cv2.convexHull( np.array( face_detected.landmarksAsXY() ).reshape((-1,2)).astype(int) ).flatten().reshape( (-1,2) )
            cv2.fillConvexPoly( hull_mask,hull,(1,1,1) )

        if self.mask_type == 'rect':
            image_mask = face_mask
        elif self.mask_type == 'faceHull':
            image_mask = hull_mask
        else:
            image_mask = ((face_mask*hull_mask))


        if self.erosion_kernel is not None:
            image_mask = cv2.erode(image_mask,self.erosion_kernel,iterations = 1)

        if self.blur_size!=0:
            image_mask = cv2.blur(image_mask,(self.blur_size,self.blur_size))

        return image_mask

    def apply_new_face(self, image, new_face, image_mask, mat, image_size, size):
        base_image = np.copy( image )
        new_image = np.copy( image )

        cv2.warpAffine( new_face, mat, image_size, new_image, cv2.WARP_INVERSE_MAP, cv2.BORDER_TRANSPARENT )

        outImage = None
        if self.seamless_clone:
            masky,maskx = cv2.transform( np.array([ size/2,size/2 ]).reshape(1,1,2) ,cv2.invertAffineTransform(mat) ).reshape(2).astype(int)
            outimage = cv2.seamlessClone(new_image.astype(np.uint8),base_image.astype(np.uint8),(image_mask*255).astype(np.uint8),(masky,maskx) , cv2.NORMAL_CLONE )
        else:
            foreground = cv2.multiply(image_mask, new_image.astype(float))
            background = cv2.multiply(1.0 - image_mask, base_image.astype(float))
            outimage = cv2.add(foreground, background)

        return outimage

    def avg_color(self, old_face, new_face):
        w = old_face.shape[0]
        h = old_face.shape[1]
        old_face = old_face.astype(float)
        new_face = new_face.astype(float)

        rank = 5
        scale = 1.0 / rank

        for i in range(rank):
            new_face[i, i:w-i, :] = scale * i * new_face[i, i:w-i, :] + scale * (rank - i) * old_face[i, i:w-i, :]
            new_face[h-i-1, i:w-i, :] = scale * i * new_face[h-i-1, i:w-i, :] + scale * (rank - i) * old_face[h-i-1, i:w-i, :]
            new_face[i+1:h-i-1, i, :] = scale * i * new_face[i+1:h-i-1, i, :] + scale * (rank - i) * old_face[i+1:h-i-1, i, :]
            new_face[i+1:h-i-1, w-i-1, :] = scale * i * new_face[i+1:h-i-1, w-i-1, :] + scale * (rank - i) * old_face[i+1:h-i-1, w-i-1, :]
        diff = old_face[rank:h-rank-1, rank:w-rank-1, :] - new_face[rank:h-rank-1, rank:w-rank-1, :]
        avg_diff = np.sum(diff, axis=(0, 1)) / ((new_face.shape[0] - 2*rank) * (new_face.shape[1] - 2*rank))
        new_face[rank:h-rank-1, rank:w-rank-1, :] += avg_diff
        np.clip(new_face, 0.0, 255.0, out=new_face)
        return np.uint8(new_face)
    
    def process_of_video(self, video_file):
        i = 0
        size = 64
        if not os.path.exists(TMP_DIR):
            os.mkdir(TMP_DIR)
        videoCapture = cv2.VideoCapture(video_file)
        fps = videoCapture.get(cv2.CAP_PROP_FPS)
        videoWrite = cv2.VideoWriter(os.path.join(TMP_DIR, "origin_face.mp4"),
                                    cv2.VideoWriter_fourcc("m", "p", "4", "v"),
                                    fps, (size, size))
        success, frame = videoCapture.read()
        
        while success:
            faces = self.preprocess(frame)
            self.all_faces.append(faces)

            for face_detected in faces:
                mat = np.array(self.get_align_mat(face_detected)).reshape(2,3) * size
                face = cv2.warpAffine( frame, mat, (size,size) )
                videoWrite.write(np.uint8(face))
            success, frame = videoCapture.read()
            i += 1
            logger.info("detected face: processing {}".format(i))
        videoWrite.release()
        ret = self.detect_image(os.path.join(TMP_DIR, "origin_face.mp4"))
        self.video_capture = cv2.VideoCapture(os.path.join(TMP_DIR, str(ret) + ".avi"))


    def predict(self, frame, src_type):
        if src_type == "image":
            faces = self.preprocess(frame)
        elif src_type == "video":
            faces = self.all_faces.pop(0)
        size = 64
        image_size = frame.shape[1], frame.shape[0]
        if not os.path.exists(TMP_DIR):
            os.mkdir(TMP_DIR)

        for face_detected in faces:
            mat = np.array(self.get_align_mat(face_detected)).reshape(2,3) * size
            face = cv2.warpAffine( frame, mat, (size,size) )
            if src_type == "image":
                image_path = os.path.join(TMP_DIR, "origin_face.jpg")
                cv2.imwrite(image_path, face)
                ret = self.detect_image(image_path)
                if self.video_capture == None:
                    self.video_capture = cv2.VideoCapture(os.path.join(TMP_DIR, str(ret) + ".avi"))
                success, new_face = self.video_capture.read()
            elif src_type == "video":
                success, new_face = self.video_capture.read()
            new_face = self.avg_color(face, new_face)
            image_mask = self.get_image_mask(frame, new_face, face_detected, mat, image_size)
            frame = self.apply_new_face(frame, new_face, image_mask, mat, image_size, size)
        
        return np.uint8(frame)

        

class DetectedFace(object):
    def __init__(self, image, x, w, y, h, landmarks):
        self.image = image
        self.x = x
        self.w = w
        self.y = y
        self.h = h
        self.landmarks = landmarks
    
    def landmarksAsXY(self):
        return [(p.x, p.y) for p in self.landmarks.parts()]
        
