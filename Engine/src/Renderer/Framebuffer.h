
namespace Rapture
{

	class Framebuffer
	{
	public:

		Framebuffer();
		~Framebuffer();

		void Invalidate();

		unsigned int getColorAID() const { return m_colorAttachmentID; }

		void setResolution(unsigned int width, unsigned int height);

		void bind();
		void unBind();

	private:
		unsigned int m_width = 1920, m_height = 1080;
		unsigned int m_framebufferID, m_colorAttachmentID, m_depthAttachmentID;


	};

}