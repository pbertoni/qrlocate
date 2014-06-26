#include "explorerFSM.h"

State::State(WorldKB* _worldKB) //: worldKB(_worldKB)
{
	//NON LA VEDEEEEEEEEEEE!!!!!!!!!!
	this->worldKB = _worldKB;
	cout << "   Create state\n ";
}

State::~State()
{
	cout << "   Delete state\n";
}

WorldKB* State::getWorldKB()
{
	return this->worldKB;
}
void State::setWorldKB(WorldKB* kb) 
{
	this->worldKB = kb;
}
// --------------------- ---------------- ---------------- ---------------- ---------------- ----------------
State1_Init::State1_Init(WorldKB* _worldKB) : State(_worldKB) {
	//WorldKB* temp = new WorldKB; //controllare che funzioni correttamente; a esempio che sovrascriva le vecchie strutture
	//printf("allocated worldkb pointer at %p\n", temp);
	//his->setWorldKB(temp);
}
State1_Init::~State1_Init() { ; }

State* State1_Init::executeState(void)
{	
	delete this;
	return new State2_QR(this->getWorldKB());
}

// --------------------- ---------------- ---------------- ---------------- ---------------- ----------------

State2_QR::State2_QR(WorldKB* _worldKB) : State(_worldKB)
{
	cout << "Entered in State 2." << endl;
	camera_id = 0;
	int camera_angle = this->getWorldKB()->getCameraAngle();
	qrStuff.temp_qr_info.message_length = -1;

	const char* filename = "out_camera_data.yml";
	CvFileStorage* fs = cvOpenFileStorage(filename, 0, CV_STORAGE_READ);
	if (fs==NULL) {
		printf("Error during calibration file loading.\n");
		exit(76);
	}
	this->intrinsic_matrix = (CvMat*)cvReadByName(fs, 0, "camera_matrix");
	this->distortion_coeffs = (CvMat*)cvReadByName(fs, 0, "distortion_coefficients");
	this->scale_factor = cvReadIntByName(fs, 0, "scale_factor");
	this->qr_size_mm = cvReadIntByName(fs, 0, "qr_size_mm");

	resetQR();

	capture.open(camera_id);
	if (!capture.isOpened()) {
		printf("Error during capture opening.\n");
		exit(1);
	  }
	Mat framet;
    capture >> framet;
    this->frameCols = framet.cols;
	cout << "Zaghen built! \n";
}

State2_QR::~State2_QR() { ; }

State* State2_QR::executeState()
{
	cout << "Stato pippo \n";
	/*
	while ( this->searching() == false && this->getWorldKB()->getCameraAngle() < CAMERA_END_ANGLE ) 
	{ // QUA VA LA CHIAMATA DI SISTEMA PER GIRARE LA TELECAMERA DI STEP GRADI; 
		this->getWorldKB()->incrementCameraAngle();
		printf("incrementing camera angle. now is %d\n", this->getWorldKB()->getCameraAngle());
	}
	this->processing();
	*/
	delete this;
	return new State3_StatusChecking(this->getWorldKB());
}

bool State2_QR::searching()
{
	resetQR();
	
	Mat frame, frame_undistort, frame_BW;
	if( !this->capture.isOpened() )
		{ perror("Error opening capture.\n"); exit(1); }

	Mat frame0;
	capture >> frame0;
	frame0.copyTo(frame);

	if (!frame.data)
		{ perror("Error loading the frame.\n"); exit(1); }

	undistort(frame, frame_undistort, intrinsic_matrix, distortion_coeffs);
	cvtColor(frame_undistort, frame_BW, CV_BGR2GRAY);

	if (!preProcessing(frame_BW))
		return false;

	int key = 0xff & waitKey(capture.isOpened() ? 50 : 500);
	if( (key & 255) == 27 )
	   exit(0);
	   
	return true; //handle
}
bool State2_QR::processing() {
	
	double side2 = pitagora((double) (this->qrStuff.temp_qr_info.x1 - this->qrStuff.temp_qr_info.x2), (double)(this->qrStuff.temp_qr_info.y1 - this->qrStuff.temp_qr_info.y2));
	double side4 = pitagora((double) (this->qrStuff.temp_qr_info.x3 - this->qrStuff.temp_qr_info.x0), (double)(this->qrStuff.temp_qr_info.y3 - this->qrStuff.temp_qr_info.y0));
	calcPerspective_Distance(side2, side4);
	return false;
}

void State2_QR::copyCorners() {

	this->qrStuff.temp_qr_info.x0 = this->qrStuff.code->corners[0].x;
	this->qrStuff.temp_qr_info.y0 = this->qrStuff.code->corners[0].y;
	this->qrStuff.temp_qr_info.x1 = this->qrStuff.code->corners[1].x;
	this->qrStuff.temp_qr_info.y1 = this->qrStuff.code->corners[1].y;
	this->qrStuff.temp_qr_info.x2 = this->qrStuff.code->corners[2].x;
	this->qrStuff.temp_qr_info.y2 = this->qrStuff.code->corners[2].y;
	this->qrStuff.temp_qr_info.x3 = this->qrStuff.code->corners[3].x;
	this->qrStuff.temp_qr_info.y3 = this->qrStuff.code->corners[3].y;
}

int State2_QR::scaleQR(double side) {
	static double fact = (double)this->scale_factor * (double)this->qr_size_mm;
	return fact/side;
}

/** Calculate perspective rotation and distance of the QR. ---------------------------------------*/
void State2_QR::calcPerspective_Distance(double side2, double side4) {

	int qr_pixel_size = average(side2, side4);
	int s2_dist = this->scaleQR(side2);
	int s4_dist = this->scaleQR(side4);
	this->qrStuff.temp_qr_info.distance = average(s2_dist, s4_dist);

	static double threshold = qr_pixel_size/double(THRESH); // progressive based on how far the QR code is (near -> increase).
	int s_LR_dist_delta = s2_dist - s4_dist;

	if (abs(s_LR_dist_delta) < threshold)
		s_LR_dist_delta = 0;

	this->qrStuff.temp_qr_info.perspective_rotation = getAngleLR(s_LR_dist_delta, this->qr_size_mm);
}

bool State2_QR::isCentered() {

	int x_center = average(qrStuff.temp_qr_info.x0, qrStuff.temp_qr_info.x2);
	printf("x_center %d\tframeCols %d\tcentering_tolerance %d\n", x_center, frameCols, centering_tolerance);
	if ( abs( x_center - frameCols ) < centering_tolerance )
		return true;
	printf("NOT centered\n");
	return false;
}

void State2_QR::printQRInfo() {
	printf("*********************************************\n");
    printf("Perspective rotation\t\t%d\n", this->qrStuff.temp_qr_info.perspective_rotation);
    printf("Distance from camera\t\t%d\n", this->qrStuff.temp_qr_info.distance);
    printf("Payload\t\t%s\n ", this->qrStuff.temp_qr_info.qr_message);
    printf("*********************************************\n\n");
}

/** Copies payload from data to info structure. --------------------------------------------------*/
void State2_QR::copyPayload() {

	this->qrStuff.temp_qr_info.message_length = MAXLENGTH;
	int payload_len = this->qrStuff.data->payload_len;
    int payloadTruncated = 0;
    if(payload_len > MAXLENGTH-1){
      payload_len = MAXLENGTH-1;  // truncate long payloads
      payloadTruncated = 1;
    }
    this->qrStuff.temp_qr_info.payload_truncated = payloadTruncated;
    memcpy(this->qrStuff.temp_qr_info.qr_message, this->qrStuff.data->payload, payload_len+1 ); // copy the '\0' too.
    this->qrStuff.temp_qr_info.qr_message[MAXLENGTH] = '\0';
}

void State2_QR::resetQR() {
	if(this->qrStuff.q)
		quirc_destroy(this->qrStuff.q);
	cout << "sono qui\n";
	this->qrStuff.q = quirc_new();
	if(!this->qrStuff.q) {
		perror("Can't create quirc object");
		exit(1);
	}

	this->qrStuff.temp_qr_info.distance = 0;
	this->qrStuff.temp_qr_info.message_length = 0;
	this->qrStuff.temp_qr_info.payload_truncated = 0;
	this->qrStuff.temp_qr_info.perspective_rotation = 0;
	this->qrStuff.temp_qr_info.vertical_rotation = 0;
	this->qrStuff.temp_qr_info.x0 = this->qrStuff.temp_qr_info.y0 = this->qrStuff.temp_qr_info.x1 = this->qrStuff.temp_qr_info.y1 = this->qrStuff.temp_qr_info.x2 = this->qrStuff.temp_qr_info.y2 = this->qrStuff.temp_qr_info.x3 = this->qrStuff.temp_qr_info.y3 = 0;
}

/** Processes QR code. ---------------------------------------------------------------------------*/
bool State2_QR::preProcessing(Mat frame_BW) {

	cv_to_quirc(this->qrStuff.q, frame_BW);
	quirc_end(this->qrStuff.q);

  int count = quirc_count(this->qrStuff.q);
  if(count == 0){ // no QR codes found.
    return false;
  }
  if(count > 1) {
	cout << "WARNING: FOUND >1 QR. CONSEGUENZE IMPREVEDIBILI SULL'ORDINAMENTO SPAZIALE" << endl;
	}

  struct quirc_code code2;
  struct quirc_data data2;
  quirc_decode_error_t err;

  quirc_extract(this->qrStuff.q, 0, &code2); // only recognize the first QR code found in the image
  err = quirc_decode(&code2, &data2);

  if(!err) {
	copyPayload();
	if(this->getWorldKB()->isQRInKB(this->qrStuff.temp_qr_info.qr_message)) {
		cout << "QR code labelled " << this->qrStuff.temp_qr_info.qr_message << "is already in KB." << endl;
		return false;
	}
	copyCorners();
	if (!isCentered())
		return false;
  }
  return true;
}

// --------------------- ---------------- ---------------- ---------------- ---------------- ----------------

State3_StatusChecking::State3_StatusChecking(WorldKB* _worldKB) : State(_worldKB)
{
	cout << "   State4_StatusChecking state\n";
}

State3_StatusChecking::~State3_StatusChecking() { ; }

State* State3_StatusChecking::executeState()
{
	cout << "Sono nello stato 3\n";
  delete this;
  return new State4_Localizing(this->getWorldKB());
}

// --------------------- ---------------- ---------------- ---------------- ---------------- ----------------

State4_Localizing::State4_Localizing(WorldKB* _worldKB) : State(_worldKB)
{
	cout << "   State5_Localizing state\n";
}

State4_Localizing::~State4_Localizing() { ; }

State* State4_Localizing::executeState()
{
  delete this;
  return NULL;
}

// --------------------- ---------------- ---------------- ---------------- ---------------- ----------------

State5_Error::State5_Error(WorldKB* _worldKB) : State(_worldKB)
{
	cout << "   State5_Localizing state\n";
}

State5_Error::~State5_Error() { ; }

State* State5_Error::executeState()
{
  delete this;
  return NULL;
}

// --------------------- ---------------- ---------------- ---------------- ---------------- ---------------- ---------------- ---------------- ---------------- ---------------- ----------------
// --------------------- ---------------- ---------------- ---------------- ---------------- ---------------- ---------------- ---------------- ---------------- ---------------- ----------------
// --------------------- ---------------- ---------------- ---------------- ---------------- ---------------- ---------------- ---------------- ---------------- ---------------- ----------------
// --------------------- ---------------- ---------------- ---------------- ---------------- ---------------- ---------------- ---------------- ---------------- ---------------- ----------------

ExplorerFSM::ExplorerFSM(int _camera_id) 
{
	this->camera_id = _camera_id;
	WorldKB* temp = new WorldKB;
	this->worldKB = *temp;
	this->currentState = new State1_Init(&(this->worldKB));
}

ExplorerFSM::~ExplorerFSM() { ; }

void ExplorerFSM::setCurrentState(State *s)
{
	currentState = s;
}

void* ExplorerFSM::runFSM()
{
	State* temp;

	while(1) {
		temp = currentState->executeState();
		if(temp)
			setCurrentState(temp);
		else {
			printf("HO FINITO\n");
			break;
		}
	}
	return NULL;
}
