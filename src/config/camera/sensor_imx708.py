from app.interfaces.capturer import ISensor


class IMX708(ISensor):
	NATIVE_2304x1296_56 = ((2304, 1296), 56, False)
	NATIVE_2304x1296_30 = ((2304, 1296), 30, False)
	HDR_1536x864_120 = ((1536, 864), 120, True)